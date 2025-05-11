#include "pch.h"
#include "renderer.h"
#include "renderer_common.h"
#include "renderer/shaders/shared.hlsl.h"

#include "d3d12/d3d12_common.h"
#include "d3d12/d3d12_backend.h"
#include "d3d12/d3d12_resource.h"

#include "acceleration_structure/bvh_builder.h"
#include "acceleration_structure/as_util.h"

#include "core/camera/camera.h"
#include "core/logger.h"
#include "core/random.h"
#include "d3d12/d3d12_query.h"

#include "imgui/imgui.h"

namespace renderer
{

	renderer_inst_t* g_renderer = nullptr;

	static frame_context_t& get_frame_context()
	{
		return g_renderer->frame_ctx[d3d12::g_d3d->swapchain.back_buffer_index];
	}

	static inline void begin_gpu_timestamp(frame_context_t& frame_ctx, ID3D12GraphicsCommandList10* command_list, const string_t& key)
	{
		ASSERT_MSG(!hashmap::find(frame_ctx.gpu_timer_queries, key), "Tried to begin a gpu timestamp that already has an entry");
		
		gpu_timer_query_t* query = hashmap::emplace_zeroed(frame_ctx.gpu_timer_queries, key);
		query->query_idx_begin = d3d12::begin_query_timestamp(command_list);

		switch (command_list->GetType())
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT: query->d3d_command_queue = d3d12::g_d3d->command_queue_direct; break;
		case D3D12_COMMAND_LIST_TYPE_COPY: query->d3d_command_queue = d3d12::g_d3d->upload_ctx.command_queue_copy; break;
		default: FATAL_ERROR("Renderer", "Tried to begin a timestamp query on an unsupported command list type"); break;
		}
	}

	static inline void end_gpu_timestamp(frame_context_t& frame_ctx, ID3D12GraphicsCommandList10* command_list, const string_t& key)
	{
		bool is_copy_queue = command_list->GetType() == D3D12_COMMAND_LIST_TYPE_COPY;
		
		gpu_timer_query_t* query = hashmap::find(frame_ctx.gpu_timer_queries, key);
		ASSERT_MSG(query, "Called end_gpu_timestamp on a gpu timer query that has not begun yet");

		query->query_idx_end = d3d12::end_query_timestamp(command_list);
	}

	static inline void readback_timestamp_queries()
	{
		d3d12::frame_context_t d3d_frame_ctx = d3d12::get_frame_context();
		frame_context_t& frame_ctx = get_frame_context();

		// Graphics queue GPU timestamp queries
		if (frame_ctx.gpu_timer_queries.size > 0)
		{
			ID3D12Resource* readback_buffer = d3d_frame_ctx.readback_buffer_timestamps;
			uint64_t* timestamp_values = (uint64_t*)d3d12::map_resource(readback_buffer);

			hashmap_iter_t iter = hashmap::get_iter(frame_ctx.gpu_timer_queries);
			while (hashmap::advance_iter(iter))
			{
				// TODO: This is dangerous, emplacing with a string key from another hashmap does not copy the string over,
				// so when the first hashmap gets released, the keys from the second map will be invalid
				// Also need to make sure that the keys are de-allocated somehow when hashmap entry is removed or reset.
				// Its also a bit difficult to de-allocate keys with arenas, so maybe I just allocate a memory pool for keys in each hashmap?
				// Should each hashmap have its own memory arena to allocate and re-use memory from for copying keys?
				// Or should the user provide a string with the correct lifetime as a key?
				// Or should the hashmap malloc the key in those cases?
				// Or should the hashmap have a function for inserting/emplacing that takes a memory arena as a parameter to alloc the key from?
				// Or should strings as keys in hashmap always have a fixed size? char[64] Maybe make a fixed size string type
				// I think I like the fixed string better. Then I dont need to manually copy the string over for every key
				gpu_timer_result_t* result = hashmap::emplace_zeroed(g_renderer->gpu_timer_results, *iter.key);
				result->timestamp_begin = timestamp_values[iter.value->query_idx_begin];
				result->timestamp_end = timestamp_values[iter.value->query_idx_end];
			}

			d3d12::unmap_resource(readback_buffer);
		}
	}

	static render_settings_shader_data_t get_default_render_settings()
	{
		render_settings_shader_data_t defaults = {};
		defaults.use_wavefront_pathtracing = true;
		defaults.use_software_rt = false;
		defaults.render_view_mode = render_view_mode::none;
		defaults.ray_max_recursion = 3;
		defaults.accumulate = true;
		defaults.cosine_weighted_diffuse = true;

		return defaults;
	}

	static void reset_color_accumulator()
	{
		// TODO: Clear accumulator UAV render target
		g_renderer->accum_count = 1;
	}

	static void create_mesh_triangle_buffer_internal(render_mesh_t& out_mesh)
	{
		ARENA_SCRATCH_SCOPE()
		{
			// Upload mesh triangle buffer
			uint64_t buffer_byte_size = sizeof(triangle_t) * out_mesh.triangle_count;
			uint64_t upload_byte_count = buffer_byte_size;
			uint64_t upload_offset = 0;
			
			// Create triangle buffer
			out_mesh.triangle_buffer = d3d12::create_buffer(ARENA_WPRINTF(arena_scratch, L"Mesh Triangle Buffer %.*ls", STRING_EXPAND(out_mesh.debug_name)).buf, buffer_byte_size);

			// Allocate and initialize triangle buffer descriptor
			out_mesh.triangle_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			d3d12::create_buffer_srv(out_mesh.triangle_buffer, out_mesh.triangle_srv, 0, buffer_byte_size);

			// Do actual data upload
			while (upload_byte_count > 0)
			{
				// CPU to upload copy
				d3d12::upload_alloc_t& upload = d3d12::begin_upload(upload_byte_count);
				memcpy(upload.ptr, PTR_OFFSET(out_mesh.triangles, upload_offset), upload.ring_buffer_alloc.byte_size);

				// Copy current chunk from upload to final GPU buffer
				upload.d3d_command_list->CopyBufferRegion(out_mesh.triangle_buffer, upload_offset, upload.d3d_resource, upload.ring_buffer_alloc.byte_offset, upload.ring_buffer_alloc.byte_size);

				// Submit upload
				d3d12::end_upload(upload);

				upload_byte_count -= upload.ring_buffer_alloc.byte_size;
				upload_offset += upload.ring_buffer_alloc.byte_size;
			}
		}
	}

	static void create_mesh_software_blas_buffer_internal(render_mesh_t& out_mesh)
	{
		ARENA_SCRATCH_SCOPE()
		{
			bvh_builder_t::build_args_t bvh_build_args = {};
			bvh_build_args.triangles = out_mesh.triangles;
			bvh_build_args.triangle_count = out_mesh.triangle_count;
			bvh_build_args.options.interval_count = 8;
			bvh_build_args.options.subdivide_single_prim = false;

			// Build the BVH with a temporary scratch memory arena, to automatically get rid of temporary allocations for the build process
			bvh_builder_t bvh_builder = {};
			bvh_builder.build(arena_scratch, bvh_build_args);

			// Extract the final BVH data using the scratch arena as well since we will upload the data to the GPU inside this arena scratch scope
			// If we wanted to keep the BVH data around on the CPU (maybe do CPU path tracing) we could do so here by using a different arena
			bvh_t mesh_bvh;
			uint64_t mesh_bvh_byte_size;
			bvh_builder.extract(arena_scratch, mesh_bvh, mesh_bvh_byte_size);

			// Keep the BVH local bounds around for creating BVH instances later when building the TLAS
			bvh_node_t* bvh_root_node = (bvh_node_t*)mesh_bvh.data;
			out_mesh.blas_min = bvh_root_node->aabb_min;
			out_mesh.blas_max = bvh_root_node->aabb_max;

			// Upload mesh BVH buffer
			uint64_t buffer_byte_size = sizeof(bvh_header_t) + mesh_bvh_byte_size;
			uint64_t upload_byte_count = buffer_byte_size;
			uint64_t upload_offset = 0;
			bool upload_header = true;

			// Create BVH buffer
			out_mesh.blas_buffer = d3d12::create_buffer(ARENA_WPRINTF(arena_scratch, L"Mesh BLAS(SW) %.*ls", STRING_EXPAND(out_mesh.debug_name)).buf, buffer_byte_size);

			// Allocate and initialize bvh buffer descriptor
			if (!d3d12::is_valid_descriptor(out_mesh.blas_srv))
			{
				out_mesh.blas_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
			d3d12::create_buffer_srv(out_mesh.blas_buffer, out_mesh.blas_srv, 0, buffer_byte_size);

			// Do actual data upload
			while (upload_byte_count > 0)
			{
				// CPU to upload copy
				d3d12::upload_alloc_t& upload = d3d12::begin_upload(upload_byte_count);

				if (upload_header)
				{
					memcpy(upload.ptr, &mesh_bvh.header, sizeof(bvh_header_t));
					memcpy(PTR_OFFSET(upload.ptr, sizeof(bvh_header_t)), mesh_bvh.data, upload.ring_buffer_alloc.byte_size - sizeof(bvh_header_t));

					upload_header = false;
				}
				else
				{
					memcpy(upload.ptr, PTR_OFFSET(mesh_bvh.data, upload_offset - sizeof(bvh_header_t)), upload.ring_buffer_alloc.byte_size);
				}

				// Copy current chunk from upload allocation to final GPU buffer
				upload.d3d_command_list->CopyBufferRegion(out_mesh.blas_buffer, upload_offset, upload.d3d_resource, upload.ring_buffer_alloc.byte_offset, upload.ring_buffer_alloc.byte_size);

				// Submit upload
				d3d12::end_upload(upload);

				upload_byte_count -= upload.ring_buffer_alloc.byte_size;
				upload_offset += upload.ring_buffer_alloc.byte_size;
			}
		}
	}

	static void create_mesh_hardware_blas_buffer_internal(render_mesh_t& out_mesh)
	{
		ARENA_SCRATCH_SCOPE()
		{
			ID3D12GraphicsCommandList10* command_list = d3d12::get_immediate_command_list();

			ID3D12Resource* blas_scratch = nullptr;
			d3d12::create_buffer_blas(ARENA_WPRINTF(arena_scratch, L"Mesh BLAS(HW) %.*ls", STRING_EXPAND(out_mesh.debug_name)).buf,
				command_list, out_mesh.triangle_buffer, out_mesh.triangle_count, sizeof(triangle_t), &blas_scratch, &out_mesh.blas_buffer);

			d3d12::execute_immediate_command_list(command_list, true);
			DX_RELEASE_OBJECT(blas_scratch);
		}
	}

	static void change_raytracing_mode()
	{
		g_renderer->change_raytracing_mode = false;

		// Flush GPU to prevent destruction of any potential in-flight resources
		d3d12::flush();

		// Switch instance buffers to correct raytracing mode
		// HW to SW
		if (g_renderer->settings.use_software_rt)
		{
			d3d12::unmap_resource(g_renderer->tlas_instance_data_buffer);
			DX_RELEASE_OBJECT(g_renderer->tlas_instance_data_buffer);
			g_renderer->tlas_instance_data_hardware = nullptr;
		}
		// SW to HW
		else
		{
			g_renderer->tlas_instance_data_buffer = d3d12::create_buffer_upload(L"HW Raytracing Instance Desc Buffer", g_renderer->instance_data_capacity * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
			g_renderer->tlas_instance_data_hardware = (D3D12_RAYTRACING_INSTANCE_DESC*)d3d12::map_resource(g_renderer->tlas_instance_data_buffer);
		}

		// Recreate every BLAS resource
		for (uint32_t i = 0; i < g_renderer->mesh_slotmap.capacity; ++i)
		{
			render_mesh_t& mesh = g_renderer->mesh_slotmap.slots[i].value;

			if (!mesh.blas_buffer)
				continue;

			DX_RELEASE_OBJECT(mesh.blas_buffer);

			// HW to SW
			if (g_renderer->settings.use_software_rt)
			{
				create_mesh_software_blas_buffer_internal(mesh);
				ASSERT_MSG(d3d12::is_valid_descriptor(mesh.blas_srv), "Tried creating a render mesh but bvh buffer SRV is invalid");
			}
			// SW to HW
			else
			{
				create_mesh_hardware_blas_buffer_internal(mesh);
			}

			ASSERT_MSG(mesh.blas_buffer, "Tried creating a render mesh but bvh buffer is null");
		}
	}

	void init(const init_params_t& init_params)
	{
		LOG_INFO("Renderer", "Init");

		g_renderer = ARENA_BOOTSTRAP(renderer_inst_t, 0);

		g_renderer->render_width = init_params.render_width;
		g_renderer->render_height = init_params.render_height;
		g_renderer->inv_render_width = 1.0f / (float)g_renderer->render_width;
		g_renderer->inv_render_height = 1.0f / (float)g_renderer->render_height;

		slotmap::init(g_renderer->texture_slotmap, g_renderer->arena);
		slotmap::init(g_renderer->mesh_slotmap, g_renderer->arena);

		// The d3d12 backend use the same arena as the front-end renderer since the renderer initializes the backend and they have the same lifetimes
		d3d12::init_params_t backend_params = {};
		backend_params.back_buffer_count = init_params.backbuffer_count;
		backend_params.vsync = init_params.vsync;
		d3d12::init(backend_params);
		
		// Create instance buffer
		// TODO: Make the instance buffer resize automatically when required, treating the MAX_INSTANCES as the theoretically highest possible instance count instead
		g_renderer->instance_data_capacity = MAX_INSTANCES;
		g_renderer->instance_data_at = 0;
		g_renderer->instance_data = ARENA_ALLOC_ARRAY_ZERO(g_renderer->arena, instance_data_t, g_renderer->instance_data_capacity);
		g_renderer->instance_buffer = d3d12::create_buffer(L"Instance Buffer", sizeof(instance_data_t) * MAX_INSTANCES);
		g_renderer->instance_buffer_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		d3d12::create_buffer_srv(g_renderer->instance_buffer, g_renderer->instance_buffer_srv, 0, sizeof(instance_data_t) * MAX_INSTANCES);
		
		// Hardware raytracing
		if (!g_renderer->settings.use_software_rt)
		{
			// TODO: Make the instance buffer resize automatically when required, treating the MAX_INSTANCES as the theoretically highest possible instance count instead
			g_renderer->tlas_instance_data_buffer = d3d12::create_buffer_upload(L"HW Raytracing Instance Desc Buffer", g_renderer->instance_data_capacity * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
			g_renderer->tlas_instance_data_hardware = (D3D12_RAYTRACING_INSTANCE_DESC*)d3d12::map_resource(g_renderer->tlas_instance_data_buffer);
		}

		g_renderer->frame_ctx = ARENA_ALLOC_ARRAY(g_renderer->arena, frame_context_t, backend_params.back_buffer_count);

		hashmap::init(g_renderer->gpu_timer_results, g_renderer->arena, d3d12::TIMESTAMP_QUERIES_DEFAULT_CAPACITY);

		// Default render settings
		g_renderer->settings = get_default_render_settings();

		// Descriptor ranges and root parameters for root signature
		D3D12_DESCRIPTOR_RANGE1 descriptor_ranges[2] = {};
		descriptor_ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptor_ranges[0].NumDescriptors = d3d12::g_d3d->descriptor_heaps.heap_sizes.cbv_srv_uav;
		descriptor_ranges[0].BaseShaderRegister = 0;
		descriptor_ranges[0].RegisterSpace = 0;
		descriptor_ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;// D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		descriptor_ranges[0].OffsetInDescriptorsFromTableStart = 0;

		descriptor_ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		descriptor_ranges[1].NumDescriptors = d3d12::g_d3d->descriptor_heaps.heap_sizes.cbv_srv_uav;
		descriptor_ranges[1].BaseShaderRegister = 0;
		descriptor_ranges[1].RegisterSpace = 0;
		descriptor_ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;// D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		descriptor_ranges[1].OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER1 root_parameters[4] = {};
		// Global render settings constant buffer
		root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[0].Descriptor.ShaderRegister = 0;
		root_parameters[0].Descriptor.RegisterSpace = 0;
		root_parameters[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		// Global view constant buffer
		root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[1].Descriptor.ShaderRegister = 1;
		root_parameters[1].Descriptor.RegisterSpace = 0;
		root_parameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		// Local shader constant buffer for accessing bindless resources and other data used for that specific shader
		root_parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[2].Descriptor.ShaderRegister = 2;
		root_parameters[2].Descriptor.RegisterSpace = 0;
		root_parameters[2].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		root_parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		root_parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		root_parameters[3].DescriptorTable.NumDescriptorRanges = ARRAY_SIZE(descriptor_ranges);
		root_parameters[3].DescriptorTable.pDescriptorRanges = descriptor_ranges;
		root_parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {};
		root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_2;
		root_signature_desc.Desc_1_2.NumParameters = ARRAY_SIZE(root_parameters);
		root_signature_desc.Desc_1_2.pParameters = root_parameters;
		root_signature_desc.Desc_1_2.NumStaticSamplers = 0;
		root_signature_desc.Desc_1_2.pStaticSamplers = nullptr;
		root_signature_desc.Desc_1_2.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

		// Root signature and PSOs
		g_renderer->root_signature = d3d12::create_root_signature(root_signature_desc);

		{
			DxcDefine defines[] =
			{
				DxcDefine{.Name = L"RAYTRACING_MODE_SOFTWARE", .Value = L"1" }
			};
			IDxcBlob* cs_binary_pathtracer_software = d3d12::compile_shader(L"shaders/pathtracer.hlsl", L"main", L"cs_6_7", ARRAY_SIZE(defines), defines);
			g_renderer->pso_cs_pathtracer_software = d3d12::create_pso_cs(cs_binary_pathtracer_software, g_renderer->root_signature);
			//DX_RELEASE_OBJECT(cs_binary_pathtracer_software);
		}

		{
			DxcDefine defines[] =
			{
				DxcDefine{ .Name = L"RAYTRACING_MODE_SOFTWARE", .Value = L"0" }
			};
			IDxcBlob* cs_binary_pathtracer_hardware = d3d12::compile_shader(L"shaders/pathtracer.hlsl", L"main", L"cs_6_7", ARRAY_SIZE(defines), defines);
			g_renderer->pso_cs_pathtracer_hardware = d3d12::create_pso_cs(cs_binary_pathtracer_hardware, g_renderer->root_signature);
			//DX_RELEASE_OBJECT(cs_binary_pathtracer_hardware);
		}

		{
			IDxcBlob* cs_binary_post_process = d3d12::compile_shader(L"shaders/post_process.hlsl", L"main", L"cs_6_7");
			g_renderer->pso_cs_post_process = d3d12::create_pso_cs(cs_binary_post_process, g_renderer->root_signature);
			//DX_RELEASE_OBJECT(cs_binary_post_process);
		}

		// Initialize wavefront pathtracing PSOs
		{
			DxcDefine defines[] = { DxcDefine{ .Name = L"RAYTRACING_MODE_SOFTWARE", .Value = L"0" } };
			IDxcBlob* shader_binary_wavefront_clear_buffers = d3d12::compile_shader(L"shaders/wavefront/clear_buffers.hlsl",
				L"main", L"cs_6_7", ARRAY_SIZE(defines), defines);
			g_renderer->wavefront.pso_clear_buffers = d3d12::create_pso_cs(shader_binary_wavefront_clear_buffers, g_renderer->root_signature);
			
			IDxcBlob* shader_binary_wavefront_init_args = d3d12::compile_shader(L"shaders/wavefront/init_args.hlsl",
				L"main", L"cs_6_7", ARRAY_SIZE(defines), defines);
			g_renderer->wavefront.pso_init_args = d3d12::create_pso_cs(shader_binary_wavefront_init_args, g_renderer->root_signature);
			
			IDxcBlob* shader_binary_wavefront_generate = d3d12::compile_shader(L"shaders/wavefront/generate.hlsl",
				L"main", L"cs_6_7", ARRAY_SIZE(defines), defines);
			g_renderer->wavefront.pso_generate = d3d12::create_pso_cs(shader_binary_wavefront_generate, g_renderer->root_signature);
			
			IDxcBlob* shader_binary_wavefront_extend = d3d12::compile_shader(L"shaders/wavefront/extend.hlsl",
				L"main", L"cs_6_7", ARRAY_SIZE(defines), defines);
			g_renderer->wavefront.pso_extend = d3d12::create_pso_cs(shader_binary_wavefront_extend, g_renderer->root_signature);
			
			IDxcBlob* shader_binary_wavefront_shade = d3d12::compile_shader(L"shaders/wavefront/shade.hlsl",
				L"main", L"cs_6_7", ARRAY_SIZE(defines), defines);
			g_renderer->wavefront.pso_shade = d3d12::create_pso_cs(shader_binary_wavefront_shade, g_renderer->root_signature);
		}

		// Initialize wavefront pathtracing resources
		{
			// Create command signature for ExecuteIndirect dispatches
			D3D12_INDIRECT_ARGUMENT_DESC indirect_arg_descs[1];
			indirect_arg_descs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
		
			D3D12_COMMAND_SIGNATURE_DESC command_signature_desc = {};
			command_signature_desc.NumArgumentDescs = ARRAY_SIZE(indirect_arg_descs);
			command_signature_desc.pArgumentDescs = indirect_arg_descs;
			command_signature_desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
			command_signature_desc.NodeMask = 0;

			d3d12::g_d3d->device->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&g_renderer->wavefront.command_signature));

			uint64_t element_count = g_renderer->render_width * g_renderer->render_height;
			// The maximum recursion count (limited by UI) is currently 8, so we allow 8 bounces for a total of 9 indirect dispatch arg sets 
			uint64_t buffer_size = 9 * sizeof(D3D12_DISPATCH_ARGUMENTS);
			g_renderer->wavefront.buffer_indirect_args = d3d12::create_buffer(L"Wavefront Indirect Arguments", buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			g_renderer->wavefront.buffer_indirect_args_srv_uav = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
			d3d12::create_buffer_srv(g_renderer->wavefront.buffer_indirect_args, g_renderer->wavefront.buffer_indirect_args_srv_uav, 0, buffer_size);
			d3d12::create_buffer_uav(g_renderer->wavefront.buffer_indirect_args, g_renderer->wavefront.buffer_indirect_args_srv_uav, 1, buffer_size);

			buffer_size = 9 * sizeof(uint32_t);
			g_renderer->wavefront.buffer_ray_counts = d3d12::create_buffer(L"Wavefront Ray Counts", buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			g_renderer->wavefront.buffer_ray_counts_srv_uav = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
			d3d12::create_buffer_srv(g_renderer->wavefront.buffer_ray_counts, g_renderer->wavefront.buffer_ray_counts_srv_uav, 0, buffer_size);
			d3d12::create_buffer_uav(g_renderer->wavefront.buffer_ray_counts, g_renderer->wavefront.buffer_ray_counts_srv_uav, 1, buffer_size);
			
			buffer_size = element_count * sizeof(RayDesc2);
			g_renderer->wavefront.buffer_ray = d3d12::create_buffer(L"Wavefront Ray Buffer", buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			g_renderer->wavefront.buffer_ray_srv_uav = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
			d3d12::create_buffer_srv(g_renderer->wavefront.buffer_ray, g_renderer->wavefront.buffer_ray_srv_uav, 0, buffer_size);
			d3d12::create_buffer_uav(g_renderer->wavefront.buffer_ray, g_renderer->wavefront.buffer_ray_srv_uav, 1, buffer_size);

			buffer_size = element_count * 12;
			g_renderer->wavefront.buffer_energy = d3d12::create_buffer(L"Wavefront Energy Buffer", buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			g_renderer->wavefront.buffer_energy_srv_uav = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
			d3d12::create_buffer_srv(g_renderer->wavefront.buffer_energy, g_renderer->wavefront.buffer_energy_srv_uav, 0, buffer_size);
			d3d12::create_buffer_uav(g_renderer->wavefront.buffer_energy, g_renderer->wavefront.buffer_energy_srv_uav, 1, buffer_size);

			buffer_size = element_count * 12;
			g_renderer->wavefront.buffer_throughput = d3d12::create_buffer(L"Wavefront Throughput Buffer", buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			g_renderer->wavefront.buffer_throughput_srv_uav = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
			d3d12::create_buffer_srv(g_renderer->wavefront.buffer_throughput, g_renderer->wavefront.buffer_throughput_srv_uav, 0, buffer_size);
			d3d12::create_buffer_uav(g_renderer->wavefront.buffer_throughput, g_renderer->wavefront.buffer_throughput_srv_uav, 1, buffer_size);

			buffer_size = element_count * 8;
			g_renderer->wavefront.buffer_pixelpos = d3d12::create_buffer(L"Wavefront Pixelpos Buffer", buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			g_renderer->wavefront.buffer_pixelpos_srv_uav = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
			d3d12::create_buffer_srv(g_renderer->wavefront.buffer_pixelpos, g_renderer->wavefront.buffer_pixelpos_srv_uav, 0, buffer_size);
			d3d12::create_buffer_uav(g_renderer->wavefront.buffer_pixelpos, g_renderer->wavefront.buffer_pixelpos_srv_uav, 1, buffer_size);

			buffer_size = element_count * sizeof(intersection_result_t);
			g_renderer->wavefront.buffer_intersection = d3d12::create_buffer(L"Wavefront Intersection Buffer", buffer_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			g_renderer->wavefront.buffer_intersection_srv_uav = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
			d3d12::create_buffer_srv(g_renderer->wavefront.buffer_intersection, g_renderer->wavefront.buffer_intersection_srv_uav, 0, buffer_size);
			d3d12::create_buffer_uav(g_renderer->wavefront.buffer_intersection, g_renderer->wavefront.buffer_intersection_srv_uav, 1, buffer_size);
		}

		// Create render targets
		{
			g_renderer->rt_color_accum = d3d12::create_texture_2d(L"Color Accumulator RT", DXGI_FORMAT_R16G16B16A16_FLOAT,
				g_renderer->render_width, g_renderer->render_height, 1,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nullptr);
			g_renderer->rt_color_accum_srv_uav = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
			d3d12::create_texture_2d_srv(g_renderer->rt_color_accum, g_renderer->rt_color_accum_srv_uav, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
			d3d12::create_texture_2d_uav(g_renderer->rt_color_accum, g_renderer->rt_color_accum_srv_uav, 1);

			g_renderer->rt_final_color = d3d12::create_texture_2d(L"Final Color RT", DXGI_FORMAT_R8G8B8A8_UNORM,
				g_renderer->render_width, g_renderer->render_height, 1,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nullptr);
			g_renderer->rt_final_color_uav = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			d3d12::create_texture_2d_uav(g_renderer->rt_final_color, g_renderer->rt_final_color_uav, 0);
		}
	}

	void exit()
	{
		ARENA_RELEASE(g_renderer->frame_arena);
		
		slotmap::destroy(g_renderer->texture_slotmap);
		slotmap::destroy(g_renderer->mesh_slotmap);
		
		// Wait for all potential in-flight frames to finish operations on the GPU
		d3d12::wait_on_fence(d3d12::g_d3d->sync.fence, d3d12::g_d3d->sync.fence_value);
		d3d12::flush_uploads();
		
		DX_RELEASE_OBJECT(g_renderer->wavefront.command_signature);
		
		DX_RELEASE_OBJECT(g_renderer->wavefront.pso_clear_buffers);
		DX_RELEASE_OBJECT(g_renderer->wavefront.pso_init_args);
		DX_RELEASE_OBJECT(g_renderer->wavefront.pso_generate);
		DX_RELEASE_OBJECT(g_renderer->wavefront.pso_extend);
		DX_RELEASE_OBJECT(g_renderer->wavefront.pso_shade);
		//DX_RELEASE_OBJECT(g_renderer->wavefront.pso_connect);
		
		DX_RELEASE_OBJECT(g_renderer->wavefront.buffer_indirect_args);
		DX_RELEASE_OBJECT(g_renderer->wavefront.buffer_ray_counts);
		DX_RELEASE_OBJECT(g_renderer->wavefront.buffer_ray);
		DX_RELEASE_OBJECT(g_renderer->wavefront.buffer_energy);
		DX_RELEASE_OBJECT(g_renderer->wavefront.buffer_throughput);
		DX_RELEASE_OBJECT(g_renderer->wavefront.buffer_pixelpos);
		DX_RELEASE_OBJECT(g_renderer->wavefront.buffer_intersection);

		DX_RELEASE_OBJECT(g_renderer->rt_color_accum);
		DX_RELEASE_OBJECT(g_renderer->rt_final_color);

		DX_RELEASE_OBJECT(g_renderer->root_signature);
		DX_RELEASE_OBJECT(g_renderer->pso_cs_pathtracer_software);
		DX_RELEASE_OBJECT(g_renderer->pso_cs_pathtracer_hardware);
		DX_RELEASE_OBJECT(g_renderer->pso_cs_post_process);
		
		d3d12::exit();
	}

	void begin_frame()
	{
		d3d12::begin_frame();

		// Read back the GPU timestamp queries from the buffer
		hashmap::clear(g_renderer->gpu_timer_results);
		readback_timestamp_queries();
		
		// Clear arena for the current frame
		ARENA_CLEAR(g_renderer->frame_arena);

		// (Re-)Initialize the gpu timestamp queries for the current frame
		frame_context_t& frame_ctx = get_frame_context();
		hashmap::init(frame_ctx.gpu_timer_queries, g_renderer->frame_arena, d3d12::TIMESTAMP_QUERIES_DEFAULT_CAPACITY);

		if (g_renderer->change_raytracing_mode)
		{
			change_raytracing_mode();
		}
		
		if (g_renderer->settings.use_software_rt)
		{
			g_renderer->tlas_instance_data_software = ARENA_ALLOC_ARRAY_ZERO(g_renderer->frame_arena, bvh_instance_t, g_renderer->instance_data_capacity);
		}

		g_renderer->cb_render_settings = d3d12::allocate_frame_resource(sizeof(render_settings_shader_data_t), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		render_settings_shader_data_t* ptr_settings = (render_settings_shader_data_t*)g_renderer->cb_render_settings.ptr;
		*ptr_settings = g_renderer->settings;
	}

	void end_frame()
	{
		d3d12::frame_context_t& d3d_frame_ctx = d3d12::get_frame_context();
		frame_context_t& frame_ctx = get_frame_context();
		
		begin_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("Copy Backbuffer"));
		d3d12::copy_to_back_buffer(g_renderer->rt_final_color, g_renderer->render_width, g_renderer->render_height);
		end_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("Copy Backbuffer"));
		
		begin_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("ImGui"));
		d3d12::render_imgui();
		end_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("ImGui"));
		
		d3d12::end_frame();
		d3d12::present();

		g_renderer->frame_index++;
		if (g_renderer->settings.accumulate)
			g_renderer->accum_count++;
	}

	void begin_scene(const camera_t& scene_camera, render_texture_handle_t env_render_texture_handle)
	{
		if (g_renderer->scene_camera.view_matrix != scene_camera.view_matrix)
		{
			reset_color_accumulator();
		}

		g_renderer->scene_camera = scene_camera;
		g_renderer->scene_hdr_env_texture = slotmap::find(g_renderer->texture_slotmap, env_render_texture_handle);
		ASSERT(g_renderer->scene_hdr_env_texture);

		// Set new camera data for the view constant buffer
		glm::mat4 proj_mat = glm::perspectiveFovLH_ZO(glm::radians(g_renderer->scene_camera.vfov_deg),
			(float)g_renderer->render_width, (float)g_renderer->render_height, 0.001f, 10000.0f);
		
		g_renderer->cb_view = d3d12::allocate_frame_resource(sizeof(view_shader_data_t), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		view_shader_data_t* view_cb = (view_shader_data_t*)g_renderer->cb_view.ptr;

		view_cb->world_to_view = g_renderer->scene_camera.view_matrix;
		view_cb->view_to_world = glm::inverse(g_renderer->scene_camera.view_matrix);
		view_cb->view_to_clip = proj_mat;
		view_cb->clip_to_view = glm::inverse(proj_mat);
		view_cb->render_dim.x = (float)g_renderer->render_width;
		view_cb->render_dim.y = (float)g_renderer->render_height;
	}

	void render()
	{
		d3d12::frame_context_t& d3d_frame_ctx = d3d12::get_frame_context();
		frame_context_t& frame_ctx = get_frame_context();

		if (g_renderer->instance_data_at > 0)
		{
			if (g_renderer->settings.use_software_rt)
			{
				ARENA_SCRATCH_SCOPE()
				{
					// Build the TLAS
					tlas_builder_t tlas_builder = {};
					tlas_builder.build(arena_scratch, g_renderer->tlas_instance_data_software, g_renderer->instance_data_at);
					uint64_t tlas_byte_size = 0;
					tlas_builder.extract(arena_scratch, g_renderer->scene_tlas, tlas_byte_size);

					// Upload TLAS to the GPU
					// Copy TLAS data from CPU to upload buffer allocation
					d3d12::frame_resource_t upload = d3d12::allocate_frame_resource(sizeof(tlas_header_t) + tlas_byte_size);

					memcpy(upload.ptr, &g_renderer->scene_tlas.header, sizeof(tlas_header_t));
					memcpy(PTR_OFFSET(upload.ptr, sizeof(tlas_header_t)), g_renderer->scene_tlas.data, tlas_byte_size);

					// Create TLAS buffer
					DX_RELEASE_OBJECT(frame_ctx.scene_tlas_resource);
					frame_ctx.scene_tlas_resource = d3d12::create_buffer(L"Scene TLAS Buffer (SW)", sizeof(bvh_header_t) + tlas_byte_size);

					// Allocate SRV for the scene TLAS, if there is none yet
					if (!d3d12::is_valid_descriptor(frame_ctx.scene_tlas_srv))
					{
						frame_ctx.scene_tlas_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					}
					// Update the scene TLAS descriptor
					d3d12::create_buffer_srv(frame_ctx.scene_tlas_resource, frame_ctx.scene_tlas_srv, 0, sizeof(bvh_header_t) + tlas_byte_size);

					// Copy TLAS data from upload buffer to final buffer
					d3d_frame_ctx.command_list->CopyBufferRegion(frame_ctx.scene_tlas_resource, 0,
						upload.resource, upload.byte_offset, sizeof(tlas_header_t) + tlas_byte_size);
				}
			}
			else
			{
				begin_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("TLAS Build"));
				// TODO: If the buffer size is sufficient, do not release it
				DX_RELEASE_OBJECT(frame_ctx.scene_tlas_scratch_resource);
				DX_RELEASE_OBJECT(frame_ctx.scene_tlas_resource);

				d3d12::create_buffer_tlas(L"Scene TLAS Buffer (HW)", d3d_frame_ctx.command_list,
					g_renderer->tlas_instance_data_buffer, g_renderer->instance_data_at,
					&frame_ctx.scene_tlas_scratch_resource, &frame_ctx.scene_tlas_resource);

				// Allocate SRV for the scene TLAS, if there is none yet
				if (!d3d12::is_valid_descriptor(frame_ctx.scene_tlas_srv))
				{
					frame_ctx.scene_tlas_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				}
				// Update the scene TLAS descriptor
				d3d12::create_as_srv(frame_ctx.scene_tlas_resource, frame_ctx.scene_tlas_srv, 0);
				end_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("TLAS Build"));
			}

			{
				// Upload instance buffer data
				uint64_t byte_size = sizeof(instance_data_t) * g_renderer->instance_data_at;
				d3d12::frame_resource_t upload = d3d12::allocate_frame_resource(byte_size);

				// CPU to upload heap copy
				memcpy(upload.ptr, g_renderer->instance_data, byte_size);

				// Upload heap to default heap copy
				d3d_frame_ctx.command_list->CopyBufferRegion(g_renderer->instance_buffer, 0, upload.resource, upload.byte_offset, byte_size);
			}
		}

		// Set descriptor heap, needs to happen before clearing UAVs
		ID3D12DescriptorHeap* descriptor_heaps[1] = { d3d12::g_d3d->descriptor_heaps.cbv_srv_uav };
		d3d_frame_ctx.command_list->SetDescriptorHeaps(ARRAY_SIZE(descriptor_heaps), descriptor_heaps);

		// Bind root signature and global constant buffers
		d3d_frame_ctx.command_list->SetComputeRootSignature(g_renderer->root_signature);
		d3d_frame_ctx.command_list->SetComputeRootConstantBufferView(0, g_renderer->cb_render_settings.resource->GetGPUVirtualAddress() + g_renderer->cb_render_settings.byte_offset);
		d3d_frame_ctx.command_list->SetComputeRootConstantBufferView(1, g_renderer->cb_view.resource->GetGPUVirtualAddress() + g_renderer->cb_view.byte_offset);

		{
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				d3d12::barrier_transition(g_renderer->rt_final_color, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			};
			d3d_frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
		}

#if 0
		// Clear render target
		float clear_value[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		// TODO: For clearing a UAV we need a handle to a shader visible heap and a non-shader visible heap.
		// I think it might be a good idea to simply have a scratch non-shader visible heap, and whenever we need to clear a UAV,
		// we simply allocate from that scratch heap and make a UAV on the spot before clearing, or only allocate one for textures used as render targets.
		frame_ctx.d3d12_command_list->ClearUnorderedAccessViewFloat(g_renderer->rt_final_color_uav.gpu,
			g_renderer->rt_final_color_uav.cpu, g_renderer->rt_final_color, clear_value, 0, nullptr);
#endif
		
		// TODO: Define these in a shared hlsl/cpp header, or somehow store that information in compiled shader
		const uint32_t dispatch_threads_per_block_x = 16;
		const uint32_t dispatch_threads_per_block_y = 16;

		uint32_t dispatch_blocks_x = MAX((g_renderer->render_width + dispatch_threads_per_block_x - 1) / dispatch_threads_per_block_x, 1);
		uint32_t dispatch_blocks_y = MAX((g_renderer->render_height + dispatch_threads_per_block_y - 1) / dispatch_threads_per_block_y, 1);

		// Dispatch wavefront pathtracing compute shaders
		if (g_renderer->settings.use_wavefront_pathtracing)
		{
			// TODO: The shaders should not be running if dispatch_id.x > ray_count, since those will be invalid, but we launch 64 threads at a time
			begin_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("Pathtrace Wavefront"));
			
			// Init indirect arguments
			{
				D3D12_RESOURCE_BARRIER barriers[] =
				{
					d3d12::barrier_transition(g_renderer->wavefront.buffer_indirect_args, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
				};
				d3d_frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
			}
			
			for (uint32_t recursion_depth = 0; recursion_depth <= g_renderer->settings.ray_max_recursion; ++recursion_depth)
			{
				if (recursion_depth == 0)
				{
					// Clear wavefront buffers
					struct shader_input_t
					{
						uint32_t buffer_ray_counts_index;
						uint32_t buffer_energy_index;
						uint32_t buffer_throughput_index;
						uint32_t buffer_pixelpos_index;
					};
					d3d12::frame_resource_t cb_shader = d3d12::allocate_frame_resource(sizeof(shader_input_t), 256);
					shader_input_t* shader_input = (shader_input_t*)cb_shader.ptr;
					shader_input->buffer_ray_counts_index = g_renderer->wavefront.buffer_ray_counts_srv_uav.offset + 1;
					shader_input->buffer_energy_index = g_renderer->wavefront.buffer_energy_srv_uav.offset + 1;
					shader_input->buffer_throughput_index = g_renderer->wavefront.buffer_throughput_srv_uav.offset + 1;
					shader_input->buffer_pixelpos_index = g_renderer->wavefront.buffer_pixelpos_srv_uav.offset + 1;

					d3d_frame_ctx.command_list->SetPipelineState(g_renderer->wavefront.pso_clear_buffers);
					d3d_frame_ctx.command_list->SetComputeRootConstantBufferView(2, cb_shader.resource->GetGPUVirtualAddress() + cb_shader.byte_offset);
					d3d_frame_ctx.command_list->Dispatch((g_renderer->render_width * g_renderer->render_height + 63) / 64, 1, 1);

					{
						D3D12_RESOURCE_BARRIER barriers[] =
						{
							d3d12::barrier_uav(g_renderer->wavefront.buffer_ray_counts),
							d3d12::barrier_uav(g_renderer->wavefront.buffer_energy),
							d3d12::barrier_uav(g_renderer->wavefront.buffer_throughput),
							d3d12::barrier_uav(g_renderer->wavefront.buffer_pixelpos)
						};
						d3d_frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
					}
				}

				{
					// Init indirect arguments
					struct shader_input_t
					{
						uint32_t recursion_depth;
						uint32_t buffer_ray_counts_index;
						uint32_t buffer_indirect_args_index;
					};
					d3d12::frame_resource_t cb_shader = d3d12::allocate_frame_resource(sizeof(shader_input_t), 256);
					shader_input_t* shader_input = (shader_input_t*)cb_shader.ptr;
					shader_input->recursion_depth = recursion_depth;
					shader_input->buffer_ray_counts_index = g_renderer->wavefront.buffer_ray_counts_srv_uav.offset;
					shader_input->buffer_indirect_args_index = g_renderer->wavefront.buffer_indirect_args_srv_uav.offset + 1;

					d3d_frame_ctx.command_list->SetPipelineState(g_renderer->wavefront.pso_init_args);
					d3d_frame_ctx.command_list->SetComputeRootConstantBufferView(2, cb_shader.resource->GetGPUVirtualAddress() + cb_shader.byte_offset);
					d3d_frame_ctx.command_list->Dispatch(1, 1, 1);

					{
						D3D12_RESOURCE_BARRIER barriers[] =
						{
							d3d12::barrier_uav(g_renderer->wavefront.buffer_indirect_args)
						};
						d3d_frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
					}
				}

				if (recursion_depth == 0)
				{
					// Generate
					struct shader_input_t
					{
						uint32_t buffer_ray_index;
					};
					d3d12::frame_resource_t cb_shader = d3d12::allocate_frame_resource(sizeof(shader_input_t), 256);
					shader_input_t* shader_input = (shader_input_t*)cb_shader.ptr;
					shader_input->buffer_ray_index = g_renderer->wavefront.buffer_ray_srv_uav.offset + 1;

					d3d_frame_ctx.command_list->SetPipelineState(g_renderer->wavefront.pso_generate);
					d3d_frame_ctx.command_list->SetComputeRootConstantBufferView(2, cb_shader.resource->GetGPUVirtualAddress() + cb_shader.byte_offset);
					d3d_frame_ctx.command_list->ExecuteIndirect(g_renderer->wavefront.command_signature, 1,
						g_renderer->wavefront.buffer_indirect_args, 0, nullptr, 0);

					D3D12_RESOURCE_BARRIER barriers[] =
					{
						d3d12::barrier_uav(g_renderer->wavefront.buffer_ray)
					};
					d3d_frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
				}
				{
					// Extend
					struct shader_input_t
					{
						uint32_t buffer_ray_counts_index;
						uint32_t buffer_ray_index;
						uint32_t buffer_intersection_index;
						uint32_t buffer_scene_tlas_index;
						uint32_t recursion_depth;
					};
					d3d12::frame_resource_t cb_shader = d3d12::allocate_frame_resource(sizeof(shader_input_t), 256);
					shader_input_t* shader_input = (shader_input_t*)cb_shader.ptr;
					shader_input->buffer_ray_counts_index = g_renderer->wavefront.buffer_ray_counts_srv_uav.offset;
					shader_input->buffer_ray_index = g_renderer->wavefront.buffer_ray_srv_uav.offset;
					shader_input->buffer_intersection_index = g_renderer->wavefront.buffer_intersection_srv_uav.offset + 1;
					shader_input->buffer_scene_tlas_index = frame_ctx.scene_tlas_srv.offset;
					shader_input->recursion_depth = recursion_depth;

					d3d_frame_ctx.command_list->SetPipelineState(g_renderer->wavefront.pso_extend);
					d3d_frame_ctx.command_list->SetComputeRootConstantBufferView(2, cb_shader.resource->GetGPUVirtualAddress() + cb_shader.byte_offset);
					d3d_frame_ctx.command_list->ExecuteIndirect(g_renderer->wavefront.command_signature, 1,
						g_renderer->wavefront.buffer_indirect_args, recursion_depth * sizeof(D3D12_DISPATCH_ARGUMENTS), nullptr, 0);
					
					D3D12_RESOURCE_BARRIER barriers[] =
					{
						d3d12::barrier_uav(g_renderer->wavefront.buffer_intersection)
					};
					d3d_frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
				}
				{
					// Shade
					struct shader_input_t
					{
						uint32_t buffer_ray_counts_index;
						uint32_t buffer_ray_index;
						uint32_t buffer_intersection_index;
						uint32_t buffer_energy_index;
						uint32_t buffer_throughput_index;
						uint32_t buffer_pixelpos_index;
						uint32_t buffer_instance_index;
						uint32_t texture_hdr_env_index;
						uint32_t texture_hdr_env_width;
						uint32_t texture_hdr_env_height;
						uint32_t random_seed;
						uint32_t recursion_depth;
					};
					d3d12::frame_resource_t cb_shader = d3d12::allocate_frame_resource(sizeof(shader_input_t), 256);
					shader_input_t* shader_input = (shader_input_t*)cb_shader.ptr;
					shader_input->buffer_ray_counts_index = g_renderer->wavefront.buffer_ray_counts_srv_uav.offset + 1;
					shader_input->buffer_ray_index = g_renderer->wavefront.buffer_ray_srv_uav.offset + 1;
					shader_input->buffer_intersection_index = g_renderer->wavefront.buffer_intersection_srv_uav.offset;
					shader_input->buffer_energy_index = g_renderer->wavefront.buffer_energy_srv_uav.offset + 1;
					shader_input->buffer_throughput_index = g_renderer->wavefront.buffer_throughput_srv_uav.offset + 1;
					shader_input->buffer_pixelpos_index = g_renderer->wavefront.buffer_pixelpos_srv_uav.offset + 1;
					shader_input->buffer_instance_index = g_renderer->instance_buffer_srv.offset;
					shader_input->texture_hdr_env_index = g_renderer->scene_hdr_env_texture->texture_srv.offset;
					shader_input->texture_hdr_env_width = g_renderer->scene_hdr_env_texture->width;
					shader_input->texture_hdr_env_height = g_renderer->scene_hdr_env_texture->height;
					shader_input->random_seed = random::rand_uint32();
					shader_input->recursion_depth = recursion_depth;

					d3d_frame_ctx.command_list->SetPipelineState(g_renderer->wavefront.pso_shade);
					d3d_frame_ctx.command_list->SetComputeRootConstantBufferView(2, cb_shader.resource->GetGPUVirtualAddress() + cb_shader.byte_offset);
					d3d_frame_ctx.command_list->ExecuteIndirect(g_renderer->wavefront.command_signature, 1,
						g_renderer->wavefront.buffer_indirect_args, recursion_depth * sizeof(D3D12_DISPATCH_ARGUMENTS), nullptr, 0);

					D3D12_RESOURCE_BARRIER barriers[] =
					{
						d3d12::barrier_uav(g_renderer->wavefront.buffer_ray_counts),
						d3d12::barrier_uav(g_renderer->wavefront.buffer_ray),
						d3d12::barrier_uav(g_renderer->wavefront.buffer_energy),
						d3d12::barrier_uav(g_renderer->wavefront.buffer_throughput),
						d3d12::barrier_uav(g_renderer->wavefront.buffer_pixelpos)
					};
					d3d_frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
				}
				{
					// TODO: Connect (Next event estimation)
				}
			}
			
			end_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("Pathtrace Wavefront"));
		}
		else
		{
			begin_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("Pathtrace Megakernel"));
			
			struct shader_input_t
			{
				uint32_t scene_tlas_index;
				uint32_t hdr_env_index;
				glm::uvec2 hdr_env_dimensions;
				uint32_t instance_buffer_index;
				uint32_t buffer_energy_index;
				uint32_t random_seed;
			};
			d3d12::frame_resource_t cb_shader = d3d12::allocate_frame_resource(sizeof(shader_input_t), 256);
			shader_input_t* shader_input = (shader_input_t*)cb_shader.ptr;
			shader_input->scene_tlas_index = frame_ctx.scene_tlas_srv.offset;
			shader_input->hdr_env_index = g_renderer->scene_hdr_env_texture->texture_srv.offset;
			shader_input->hdr_env_dimensions = glm::uvec2(g_renderer->scene_hdr_env_texture->width, g_renderer->scene_hdr_env_texture->height);
			shader_input->instance_buffer_index = g_renderer->instance_buffer_srv.offset;
			shader_input->buffer_energy_index = g_renderer->wavefront.buffer_energy_srv_uav.offset + 1;
			shader_input->random_seed = random::rand_uint32();

			d3d_frame_ctx.command_list->SetPipelineState(g_renderer->pso_cs_pathtracer_hardware);
			d3d_frame_ctx.command_list->SetComputeRootConstantBufferView(2, cb_shader.resource->GetGPUVirtualAddress() + cb_shader.byte_offset);
			d3d_frame_ctx.command_list->Dispatch(dispatch_blocks_x, dispatch_blocks_y, 1);

			D3D12_RESOURCE_BARRIER barriers[] =
			{
				d3d12::barrier_uav(g_renderer->wavefront.buffer_energy)
			};
			d3d_frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
			
			end_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("Pathtrace Megakernel"));
		}

		// Dispatch post-process compute shader
		{
			begin_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("Post-Process"));

			// Bind shader input constant buffer
			struct shader_input_t
			{
				uint32_t buffer_energy_index;
				uint32_t texture_color_accum_index;
				uint32_t texture_color_final_index;
				uint32_t sample_count;
			};
			d3d12::frame_resource_t shader_cb = d3d12::allocate_frame_resource(sizeof(shader_input_t), 256);
			shader_input_t* shader_input = (shader_input_t*)shader_cb.ptr;
			shader_input->buffer_energy_index = g_renderer->wavefront.buffer_energy_srv_uav.offset;
			shader_input->texture_color_accum_index = g_renderer->rt_color_accum_srv_uav.offset + 1;
			shader_input->texture_color_final_index = g_renderer->rt_final_color_uav.offset;
			shader_input->sample_count = g_renderer->accum_count;

			d3d_frame_ctx.command_list->SetPipelineState(g_renderer->pso_cs_post_process);
			d3d_frame_ctx.command_list->SetComputeRootConstantBufferView(2, shader_cb.resource->GetGPUVirtualAddress() + shader_cb.byte_offset);
			d3d_frame_ctx.command_list->Dispatch(dispatch_blocks_x, dispatch_blocks_y, 1);

			D3D12_RESOURCE_BARRIER barriers[] =
			{
				d3d12::barrier_uav(g_renderer->rt_color_accum),
				d3d12::barrier_uav(g_renderer->rt_final_color)
			};
			d3d_frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
			
			end_gpu_timestamp(frame_ctx, d3d_frame_ctx.command_list, STRING_LITERAL("Post-Process"));
		}
	}

	void end_scene()
	{
		g_renderer->instance_data_at = 0;
	}

	void render_ui()
	{
		if (ImGui::Begin("Renderer"))
		{
			bool should_reset_accumulators = false;

			ImGui::Text("Resolution: %ux%u", g_renderer->render_width, g_renderer->render_height);
			// GPU timestamps increase when vsync is enabled. Seems that the driver is doing stalls related to VSync that affect timings
			// See https://gamedev.net/forums/topic/706827-issue-with-directx12-timestamps/
			ImGui::Checkbox("VSync", &d3d12::g_d3d->vsync);
			ImGui::SetItemTooltip("When VSync is enabled, GPU timers cannot be trusted because depending on hardware/drivers,"
						 "any stalls introduced by VSync might affect them.");
			ImGui::Text("Accumulator: %u", g_renderer->accum_count);

			if (ImGui::CollapsingHeader("GPU Timers"))
			{
				// TODO: Get the correct timestamp frequency for each command queue, maybe store it inside the gpu timers
				double timestamp_freq = (double)d3d12::get_timestamp_frequency();

				hashmap_iter_t iter = hashmap::get_iter(g_renderer->gpu_timer_results);
				while (hashmap::advance_iter(iter))
				{
					double timer_elapsed = (double)(iter.value->timestamp_end - iter.value->timestamp_begin);
					double timer_ms = (timer_elapsed / timestamp_freq) * 1000.0;
					
					ImGui::Text("%.*s: %.3f ms", STRING_EXPAND(*iter.key), timer_ms);
				}
			}

			// GPU memory usage
			if (ImGui::CollapsingHeader("GPU Memory"))
			{
				d3d12::device_memory_info_t gpu_memory = d3d12::get_device_memory_info();
				
				ImGui::SeparatorText("Dedicated Memory");
				ImGui::Text("Usage: %llu MB", TO_MB(gpu_memory.local_mem.CurrentUsage));
				ImGui::Text("Available: %llu MB", TO_MB(gpu_memory.local_mem.Budget - gpu_memory.local_mem.CurrentUsage));
				ImGui::Text("Reserved: %llu MB", TO_MB(gpu_memory.local_mem.CurrentReservation));
				ImGui::Text("Total: %llu MB", TO_MB(gpu_memory.local_mem.Budget));
				ImGui::Text("Available reserve: %llu MB", TO_MB(gpu_memory.local_mem.AvailableForReservation));

				ImGui::SeparatorText("Shared Memory");
				ImGui::Text("Usage: %llu MB", TO_MB(gpu_memory.non_local_mem.CurrentUsage));
				ImGui::Text("Available: %llu MB", TO_MB(gpu_memory.non_local_mem.Budget - gpu_memory.non_local_mem.CurrentUsage));
				ImGui::Text("Reserved: %llu MB", TO_MB(gpu_memory.non_local_mem.CurrentReservation));
				ImGui::Text("Total: %llu MB", TO_MB(gpu_memory.non_local_mem.Budget));
				ImGui::Text("Available reserve: %llu MB", TO_MB(gpu_memory.non_local_mem.AvailableForReservation));
			}

			// Debug category
			if (ImGui::CollapsingHeader("Debug"))
			{
				ImGui::Indent(10.0f);

				// Render data visualization
				ImGui::Text("Render data visualization mode");
				if (ImGui::BeginCombo("##Render data visualization mode", render_view_mode_labels[g_renderer->settings.render_view_mode], ImGuiComboFlags_None))
				{
					for (uint32_t i = 0; i < render_view_mode::count; ++i)
					{
						bool selected = i == g_renderer->settings.render_view_mode;

						if (ImGui::Selectable(render_view_mode_labels[i], selected))
						{
							g_renderer->settings.render_view_mode = (render_view_mode)i;
							should_reset_accumulators = true;
						}

						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::Unindent(10.0f);
			}

			// Render Settings
			if (ImGui::CollapsingHeader("Settings"))
			{
				ImGui::Indent(10.0f);

				// Wavefront or megakernel pathtracing
				ImGui::Checkbox("Use Wavefront Path-tracing", (bool*)&g_renderer->settings.use_wavefront_pathtracing);
				ImGui::SetItemTooltip("On: Wavefront path-tracing enabled.\nOff: Megakernel path-tracing enabled");

				// Software/Hardware raytracing
				ImGui::BeginDisabled(true);
				if (ImGui::Checkbox("Use software raytracing", (bool*)&g_renderer->settings.use_software_rt))
				{
					g_renderer->change_raytracing_mode = true;
				}
				ImGui::EndDisabled();
				ImGui::SetItemTooltip("When this is enabled the raytracing will use software raytracing with custom acceleration structures instead of hardware raytracing."
					"This might take a while because it will rebuild all bottom level acceleration structures of all meshes.");

				// Slider for maximum amount of recursion for each ray
				if (ImGui::SliderInt("Ray max recursion",(int32_t*)&g_renderer->settings.ray_max_recursion, 0, 8)) should_reset_accumulators = true;
				// Toggle accumulation
				if (ImGui::Checkbox("Accumulate", (bool*)&g_renderer->settings.accumulate)) should_reset_accumulators = true;
				// Enable/disable cosine weighted diffuse reflections, uses uniform hemisphere sample if disabled
				if (ImGui::Checkbox("Cosine weighted diffuse", (bool*)&g_renderer->settings.cosine_weighted_diffuse)) should_reset_accumulators = true;

				ImGui::Unindent(10.0f);
			}

			if (should_reset_accumulators)
				reset_color_accumulator();
		}

		ImGui::End();
	}

	render_texture_handle_t create_render_texture(const render_texture_params_t& texture_params)
	{
		// TODO: Support different DXGI formats, figure out format based on texture_params
		// TODO: Support other texture dimensions than 2D
		// TODO: Support uploading textures with mips
		// TODO: Support mip generation
		// TODO: Support compressed textures
		
		ID3D12Resource* d3d_resource = nullptr;
		ARENA_SCRATCH_SCOPE()
		{
			d3d_resource = d3d12::create_texture_2d(ARENA_WPRINTF(arena_scratch, L"Texture Buffer %.*ls", STRING_EXPAND(texture_params.debug_name)).buf,
				DXGI_FORMAT_R32G32B32A32_FLOAT, texture_params.width, texture_params.height, 1, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
		}
		uint32_t src_rowpitch = texture_params.width * texture_params.bytes_per_channel * texture_params.channel_count;

		// Prepare texture copy destination
		D3D12_RESOURCE_DESC dst_desc = d3d_resource->GetDesc();
		D3D12_SUBRESOURCE_FOOTPRINT dst_footprint = {};
		dst_footprint.Format = dst_desc.Format;
		dst_footprint.Width = (uint32_t)dst_desc.Width;
		dst_footprint.Height = dst_desc.Height;
		dst_footprint.Depth = dst_desc.DepthOrArraySize;
		dst_footprint.RowPitch = ALIGN_UP_POW2(src_rowpitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_TEXTURE_COPY_LOCATION dst_loc = {};
		dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_loc.pResource = d3d_resource;
		dst_loc.SubresourceIndex = 0;

		// Upload in chunks of rows if the upload ring buffer cannot accomodate the entire texture at once
		uint32_t rows_to_copy = dst_desc.Height;
		uint32_t row_curr = 0;

		while (rows_to_copy > 0)
		{
			// Allocate a chunk of CPU writable memory from the upload ring buffer, with a minimum required size of the destination row pitch
			uint64_t required_bytes = MAX(rows_to_copy * dst_footprint.RowPitch, dst_footprint.RowPitch);
			d3d12::upload_alloc_t& upload = d3d12::begin_upload(required_bytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

			// Determine how many rows we can copy over with the upload allocation we got back, it might have returned less memory
			uint32_t row_count_upload = upload.ring_buffer_alloc.byte_size / dst_footprint.RowPitch;
			ASSERT_MSG(row_count_upload > 0, "Uploading texture failed because upload ring buffer is not large enough to upload a single row"
				"Texture: %ls, Width: %u, Height: %u, Mips: %u, Bytes per channel: %u, Channels: %u",
				texture_params.debug_name, texture_params.width, texture_params.height, 1, texture_params.bytes_per_channel, texture_params.channel_count);

			// Copy texture data to the upload allocation
			uint8_t* ptr_src = texture_params.ptr_data + src_rowpitch * row_curr;
			uint8_t* ptr_dst = (uint8_t*)upload.ptr;

			for (uint32_t y = 0; y < row_count_upload; ++y)
			{
				memcpy(ptr_dst, ptr_src, dst_footprint.RowPitch);
				ptr_src += src_rowpitch;
				ptr_dst += dst_footprint.RowPitch;
			}

			// Update the source footprint and source copy location with the current upload allocation
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT src_footprint = {};
			src_footprint.Footprint = dst_footprint;
			src_footprint.Footprint.Height = row_count_upload;
			src_footprint.Offset = upload.ring_buffer_alloc.byte_offset;

			D3D12_TEXTURE_COPY_LOCATION src_loc = {};
			src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src_loc.pResource = upload.d3d_resource;
			src_loc.PlacedFootprint = src_footprint;

			upload.d3d_command_list->CopyTextureRegion(&dst_loc, 0, row_curr, 0, &src_loc, nullptr);
			d3d12::end_upload(upload);

			// Update rows that are left to upload
			rows_to_copy -= row_count_upload;
			row_curr += row_count_upload;
		}

		render_texture_t render_texture = {};
		render_texture.texture_buffer = d3d_resource;
		render_texture.texture_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		render_texture.width = (uint32_t)dst_desc.Width;
		render_texture.height = dst_desc.Height;
		render_texture.depth = dst_desc.DepthOrArraySize;
		d3d12::create_texture_2d_srv(render_texture.texture_buffer, render_texture.texture_srv, 0, DXGI_FORMAT_R32G32B32A32_FLOAT);

		render_texture_handle_t handle = slotmap::add(g_renderer->texture_slotmap, render_texture);
		return handle;
	}

	render_mesh_handle_t create_render_mesh(const render_mesh_params_t& mesh_params)
	{
		render_mesh_t mesh = {};
		// Need to copy the string buffer since mesh_params.debug_name is temporary
		ARENA_COPY_WSTR(g_renderer->arena, mesh_params.debug_name, mesh.debug_name);
		mesh.triangle_count = mesh_params.index_count / 3;
		mesh.triangles = ARENA_ALLOC_ARRAY(g_renderer->arena, triangle_t, mesh.triangle_count);
		for (uint32_t tri_idx = 0, i = 0; tri_idx < mesh.triangle_count; ++tri_idx, i += 3)
		{
			mesh.triangles[tri_idx].v0 = mesh_params.vertices[mesh_params.indices[i]];
			mesh.triangles[tri_idx].v1 = mesh_params.vertices[mesh_params.indices[i + 1]];
			mesh.triangles[tri_idx].v2 = mesh_params.vertices[mesh_params.indices[i + 2]];
		}

		ARENA_SCRATCH_SCOPE()
		{
			create_mesh_triangle_buffer_internal(mesh);
			ASSERT_MSG(mesh.triangle_buffer, "Tried creating a render mesh but triangle buffer is null");
			ASSERT_MSG(d3d12::is_valid_descriptor(mesh.triangle_srv), "Tried creating a render mesh but triangle buffer SRV is invalid");

			if (g_renderer->settings.use_software_rt)
			{
				create_mesh_software_blas_buffer_internal(mesh);
				ASSERT_MSG(d3d12::is_valid_descriptor(mesh.blas_srv), "Tried creating a render mesh but bvh buffer SRV is invalid");
			}
			else
			{
				create_mesh_hardware_blas_buffer_internal(mesh);
			}

			ASSERT_MSG(mesh.blas_buffer, "Tried creating a render mesh but bvh buffer is null");
		}

		render_mesh_handle_t handle = slotmap::add(g_renderer->mesh_slotmap, mesh);
		return handle;
	}

	void submit_render_mesh(render_mesh_handle_t render_mesh_handle, const glm::mat4& transform, const material_t& material)
	{
		const render_mesh_t* mesh = slotmap::find(g_renderer->mesh_slotmap, render_mesh_handle);

		ASSERT_MSG(mesh, "Mesh with render mesh handle { index: %u, version: %u } was not valid", render_mesh_handle.index, render_mesh_handle.version);
		ASSERT_MSG(g_renderer->instance_data_at < g_renderer->instance_data_capacity, "Exceeded capacity of instances");

		// Write instance to instance buffer
		instance_data_t& instance_data = g_renderer->instance_data[g_renderer->instance_data_at];
		instance_data.local_to_world = transform;
		instance_data.world_to_local = glm::inverse(transform);
		instance_data.material = material;
		instance_data.triangle_buffer_idx = mesh->triangle_srv.offset;

		if (g_renderer->settings.use_software_rt)
		{
			bvh_instance_t* tlas_instance_software = &g_renderer->tlas_instance_data_software[g_renderer->instance_data_at];
			tlas_instance_software->world_to_local = instance_data.world_to_local;
			tlas_instance_software->bvh_index = mesh->blas_srv.offset;

			for (uint32_t i = 0; i < 8; ++i)
			{
				glm::vec3 pos_world = transform *
					glm::vec4(i & 1 ? mesh->blas_max.x : mesh->blas_min.x, i & 2 ? mesh->blas_max.y : mesh->blas_min.y, i & 4 ? mesh->blas_max.z : mesh->blas_min.z, 1.0f);
				as_util::grow_aabb(tlas_instance_software->aabb_min, tlas_instance_software->aabb_max, pos_world);
			}
		}
		else
		{
			D3D12_RAYTRACING_INSTANCE_DESC& tlas_instance_hardware = g_renderer->tlas_instance_data_hardware[g_renderer->instance_data_at];
			glm::mat4 temp = glm::transpose(transform);
			memcpy(tlas_instance_hardware.Transform, &temp, sizeof(float) * 12);
			tlas_instance_hardware.InstanceID = g_renderer->instance_data_at;
			tlas_instance_hardware.InstanceMask = 1;
			tlas_instance_hardware.InstanceContributionToHitGroupIndex = 0;
			tlas_instance_hardware.AccelerationStructure = mesh->blas_buffer->GetGPUVirtualAddress();
			tlas_instance_hardware.Flags = 0; // D3D12_RAYTRACING_INSTANCE_FLAGS
		}

		g_renderer->instance_data_at++;
	}

}
