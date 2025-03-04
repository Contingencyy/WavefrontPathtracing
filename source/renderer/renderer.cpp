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

#include "imgui/imgui.h"

namespace renderer
{

	renderer_inst_t* g_renderer = nullptr;

	static render_settings_shader_data_t get_default_render_settings()
	{
		render_settings_shader_data_t defaults = {};
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

	void init(uint32_t render_width, uint32_t render_height)
	{
		LOG_INFO("Renderer", "Init");

		g_renderer = ARENA_BOOTSTRAP(renderer_inst_t, 0);

		g_renderer->render_width = render_width;
		g_renderer->render_height = render_height;
		g_renderer->inv_render_width = 1.0f / (float)g_renderer->render_width;
		g_renderer->inv_render_height = 1.0f / (float)g_renderer->render_height;

		slotmap::init(g_renderer->texture_slotmap, &g_renderer->arena);
		slotmap::init(g_renderer->mesh_slotmap, &g_renderer->arena);

		// The d3d12 backend use the same arena as the front-end renderer since the renderer initializes the backend and they have the same lifetimes
		d3d12::backend_init_params backend_params = {};
		backend_params.arena = &g_renderer->arena;
		backend_params.back_buffer_count = 2u;
		d3d12::init(backend_params);

		g_renderer->bvh_instances_count = TLAS_MAX_BVH_INSTANCES;
		g_renderer->bvh_instances_at = 0;
		g_renderer->bvh_instances = ARENA_ALLOC_ARRAY_ZERO(&g_renderer->arena, bvh_instance_t, g_renderer->bvh_instances_count);

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

		IDxcBlob* cs_binary_pathtracer = d3d12::compile_shader(L"shaders/pathtracer.hlsl", L"main", L"cs_6_7");
		g_renderer->pso_cs_pathtracer = d3d12::create_pso_cs(cs_binary_pathtracer, g_renderer->root_signature);
		//DX_RELEASE_OBJECT(cs_binary_pathtracer);

		IDxcBlob* cs_binary_post_process = d3d12::compile_shader(L"shaders/post_process.hlsl", L"main", L"cs_6_7");
		g_renderer->pso_cs_post_process = d3d12::create_pso_cs(cs_binary_post_process, g_renderer->root_signature);
		//DX_RELEASE_OBJECT(cs_binary_post_process);

		// Render target
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

		// Create instance buffer
		g_renderer->instance_data = ARENA_ALLOC_ARRAY_ZERO(&g_renderer->arena, instance_data_t, g_renderer->bvh_instances_count);
		g_renderer->instance_buffer = d3d12::create_buffer(L"Instance Buffer", sizeof(instance_data_t) * TLAS_MAX_BVH_INSTANCES);
		g_renderer->instance_buffer_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		d3d12::create_buffer_srv(g_renderer->instance_buffer, g_renderer->instance_buffer_srv, 0, sizeof(instance_data_t) * TLAS_MAX_BVH_INSTANCES);
	}

	void exit()
	{
		slotmap::destroy(g_renderer->texture_slotmap);
		slotmap::destroy(g_renderer->mesh_slotmap);

		// Wait for all potential in-flight frames to finish operations on the GPU
		d3d12::wait_on_fence(d3d12::g_d3d->sync.fence, d3d12::g_d3d->sync.fence_value);
		d3d12::flush_uploads();

		DX_RELEASE_OBJECT(g_renderer->rt_color_accum);
		DX_RELEASE_OBJECT(g_renderer->rt_final_color);

		DX_RELEASE_OBJECT(g_renderer->root_signature);
		DX_RELEASE_OBJECT(g_renderer->pso_cs_pathtracer);
		DX_RELEASE_OBJECT(g_renderer->pso_cs_post_process);

		d3d12::exit();
	}

	void begin_frame()
	{
		d3d12::begin_frame();

		g_renderer->cb_render_settings = d3d12::allocate_frame_resource(sizeof(render_settings_shader_data_t), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		render_settings_shader_data_t* ptr_settings = (render_settings_shader_data_t*)g_renderer->cb_render_settings.ptr;
		*ptr_settings = g_renderer->settings;
	}

	void end_frame()
	{
		d3d12::copy_to_back_buffer(g_renderer->rt_final_color, g_renderer->render_width, g_renderer->render_height);

		d3d12::end_frame();
		d3d12::present();

		g_renderer->frame_index++;
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
		d3d12::frame_context_t& frame_ctx = d3d12::get_frame_context();

		if (g_renderer->bvh_instances_at > 0)
		{
			ARENA_SCRATCH_SCOPE()
			{
				// Build the TLAS
				// TODO: Do not rebuild the TLAS every single frame
				tlas_builder_t tlas_builder = {};
				tlas_builder.build(arena_scratch, g_renderer->bvh_instances, g_renderer->bvh_instances_at);
				uint64_t tlas_byte_size = 0;
				tlas_builder.extract(arena_scratch, g_renderer->scene_tlas, tlas_byte_size);

				// Upload TLAS to the GPU
				// Copy TLAS data from CPU to upload buffer allocation
				d3d12::frame_resource_t upload = d3d12::allocate_frame_resource(sizeof(tlas_header_t) + tlas_byte_size);

				memcpy(upload.ptr, &g_renderer->scene_tlas.header, sizeof(tlas_header_t));
				memcpy(PTR_OFFSET(upload.ptr, sizeof(tlas_header_t)), g_renderer->scene_tlas.data, tlas_byte_size);

				// Create/resize TLAS buffer if required
				if (!g_renderer->scene_tlas_resource || g_renderer->scene_tlas_resource->GetDesc().Width < sizeof(bvh_header_t) + tlas_byte_size)
				{
					// Release old TLAS resource and create a replacement with the right size
					DX_RELEASE_OBJECT(g_renderer->scene_tlas_resource);
					g_renderer->scene_tlas_resource = d3d12::create_buffer(L"Scene TLAS Buffer", sizeof(bvh_header_t) + tlas_byte_size);

					// Allocate SRV for the scene TLAS, if there is none yet
					if (!d3d12::is_valid_descriptor(g_renderer->scene_tlas_srv))
					{
						g_renderer->scene_tlas_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					}
					// Update the scene TLAS descriptor
					d3d12::create_buffer_srv(g_renderer->scene_tlas_resource, g_renderer->scene_tlas_srv, 0, sizeof(bvh_header_t) + tlas_byte_size);
				}

				// Copy TLAS data from upload buffer to final buffer
				frame_ctx.command_list->CopyBufferRegion(g_renderer->scene_tlas_resource, 0,
					upload.resource, upload.byte_offset, sizeof(tlas_header_t) + tlas_byte_size);
			}

			{
				// Upload instance buffer data
				uint64_t byte_size = sizeof(instance_data_t) * g_renderer->bvh_instances_at;
				d3d12::frame_resource_t upload = d3d12::allocate_frame_resource(byte_size);

				// CPU to upload heap copy
				memcpy(upload.ptr, g_renderer->instance_data, byte_size);

				// Upload heap to default heap copy
				frame_ctx.command_list->CopyBufferRegion(g_renderer->instance_buffer, 0, upload.resource, upload.byte_offset, byte_size);
			}
		}

		// Set descriptor heap, needs to happen before clearing UAVs
		ID3D12DescriptorHeap* descriptor_heaps[1] = { d3d12::g_d3d->descriptor_heaps.cbv_srv_uav };
		frame_ctx.command_list->SetDescriptorHeaps(ARRAY_SIZE(descriptor_heaps), descriptor_heaps);

		// Bind root signature and global constant buffers
		frame_ctx.command_list->SetComputeRootSignature(g_renderer->root_signature);
		frame_ctx.command_list->SetComputeRootConstantBufferView(0, g_renderer->cb_render_settings.resource->GetGPUVirtualAddress() + g_renderer->cb_render_settings.byte_offset);
		frame_ctx.command_list->SetComputeRootConstantBufferView(1, g_renderer->cb_view.resource->GetGPUVirtualAddress() + g_renderer->cb_view.byte_offset);

		{
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				d3d12::barrier_transition(g_renderer->rt_final_color, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			};
			frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
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

		// Dispatch pathtrace compute shader
		{
			// TODO: Define these in a shared hlsl/cpp header, or set these as a define when compiling the shaders/PSO
			const uint32_t dispatch_threads_per_block_x = 16;
			const uint32_t dispatch_threads_per_block_y = 16;

			uint32_t dispatch_blocks_x = MAX((g_renderer->render_width + dispatch_threads_per_block_x - 1) / dispatch_threads_per_block_x, 1);
			uint32_t dispatch_blocks_y = MAX((g_renderer->render_height + dispatch_threads_per_block_y - 1) / dispatch_threads_per_block_y, 1);

			frame_ctx.command_list->SetPipelineState(g_renderer->pso_cs_pathtracer);

			// Bind shader input constant buffer
			struct shader_input_t
			{
				uint32_t scene_tlas_index;
				uint32_t hdr_env_index;
				uint32_t instance_buffer_index;
				uint32_t rt_index;
				uint32_t sample_count;
				uint32_t random_seed;
			};

			d3d12::frame_resource_t cb_shader = d3d12::allocate_frame_resource(sizeof(shader_input_t), 256);
			shader_input_t* shader_input = (shader_input_t*)cb_shader.ptr;

			shader_input->scene_tlas_index = g_renderer->scene_tlas_srv.offset;
			shader_input->hdr_env_index = g_renderer->scene_hdr_env_texture->texture_srv.offset;
			shader_input->instance_buffer_index = g_renderer->instance_buffer_srv.offset;
			shader_input->rt_index = g_renderer->rt_color_accum_srv_uav.offset + 1;
			shader_input->random_seed = random::rand_uint32();
			shader_input->sample_count = g_renderer->accum_count;
			
			frame_ctx.command_list->SetComputeRootConstantBufferView(2, cb_shader.resource->GetGPUVirtualAddress() + cb_shader.byte_offset);
			frame_ctx.command_list->Dispatch(dispatch_blocks_x, dispatch_blocks_y, 1);
		}

		// Dispatch post-process compute shader
		{
			const uint32_t dispatch_threads_per_block_x = 16;
			const uint32_t dispatch_threads_per_block_y = 16;

			uint32_t dispatch_blocks_x = MAX((g_renderer->render_width + dispatch_threads_per_block_x - 1) / dispatch_threads_per_block_x, 1);
			uint32_t dispatch_blocks_y = MAX((g_renderer->render_height + dispatch_threads_per_block_y - 1) / dispatch_threads_per_block_y, 1);

			frame_ctx.command_list->SetComputeRootSignature(g_renderer->root_signature);
			frame_ctx.command_list->SetPipelineState(g_renderer->pso_cs_post_process);

			// Bind shader input constant buffer
			struct shader_input_t
			{
				uint32_t color_index;
				uint32_t rt_index;
			};

			d3d12::frame_resource_t shader_cb = d3d12::allocate_frame_resource(sizeof(shader_input_t), 256);
			shader_input_t* shader_input = (shader_input_t*)shader_cb.ptr;
			shader_input->color_index = g_renderer->rt_color_accum_srv_uav.offset;
			shader_input->rt_index = g_renderer->rt_final_color_uav.offset;

			frame_ctx.command_list->SetComputeRootConstantBufferView(2, shader_cb.resource->GetGPUVirtualAddress() + shader_cb.byte_offset);
			frame_ctx.command_list->Dispatch(dispatch_blocks_x, dispatch_blocks_y, 1);
		}
	}

	void end_scene()
	{
		g_renderer->bvh_instances_at = 0;
	}

	void render_ui()
	{
		if (ImGui::Begin("Renderer"))
		{
			bool should_reset_accumulators = false;

			//ImGui::Text("render time: %.3f ms", Profiler::GetTimerResult(GlobalProfilingScope_RenderTime).lastElapsed * 1000.0f);

			ImGui::Text("Resolution: %ux%u", g_renderer->render_width, g_renderer->render_height);
			ImGui::Text("Accumulator: %u", g_renderer->accum_count);

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
		
		ID3D12Resource* d3d_resource = nullptr;
		ARENA_SCRATCH_SCOPE()
		{
			d3d_resource = d3d12::create_texture_2d(ARENA_WPRINTF(arena_scratch, L"Texture Buffer %ls", texture_params.debug_name).buf,
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
		d3d12::create_texture_2d_srv(render_texture.texture_buffer, render_texture.texture_srv, 0, DXGI_FORMAT_R32G32B32A32_FLOAT);

		render_texture_handle_t handle = slotmap::add(g_renderer->texture_slotmap, render_texture);
		return handle;
	}

	render_mesh_handle_t create_render_mesh(const render_mesh_params_t& mesh_params)
	{
		bvh_builder_t::build_args_t bvh_build_args = {};
		bvh_build_args.vertex_count = mesh_params.vertex_count;
		bvh_build_args.vertices = mesh_params.vertices;
		bvh_build_args.index_count = mesh_params.index_count;
		bvh_build_args.indices = mesh_params.indices;
		bvh_build_args.options.interval_count = 8;
		bvh_build_args.options.subdivide_single_prim = false;

		render_mesh_handle_t handle = {};

		ARENA_SCRATCH_SCOPE()
		{
			// Build the BVH with a temporary scratch memory arena, to automatically get rid of temporary allocations for the build process
			bvh_builder_t bvh_builder = {};
			bvh_builder.build(arena_scratch, bvh_build_args);

			// Extract the final BVH data using the scratch arena as well since we will upload the data to the GPU inside this arena scratch scope
			// If we wanted to keep the BVH data around on the CPU (maybe do CPU path tracing) we could do so here by using a different arena
			bvh_t mesh_bvh;
			uint64_t mesh_bvh_byte_size;
			bvh_builder.extract(arena_scratch, mesh_bvh, mesh_bvh_byte_size);

			uint32_t mesh_triangle_count = mesh_params.index_count / 3;
			triangle_t* mesh_triangles = ARENA_ALLOC_ARRAY(arena_scratch, triangle_t, mesh_triangle_count);
			uint64_t mesh_triangles_byte_size = sizeof(triangle_t) * mesh_triangle_count;

			for (uint32_t tri_idx = 0, i = 0; tri_idx < mesh_triangle_count; ++tri_idx, i += 3)
			{
				mesh_triangles[tri_idx].p0 = mesh_params.vertices[mesh_params.indices[i]].position;
				mesh_triangles[tri_idx].p1 = mesh_params.vertices[mesh_params.indices[i + 1]].position;
				mesh_triangles[tri_idx].p2 = mesh_params.vertices[mesh_params.indices[i + 2]].position;

				mesh_triangles[tri_idx].n0 = mesh_params.vertices[mesh_params.indices[i]].normal;
				mesh_triangles[tri_idx].n1 = mesh_params.vertices[mesh_params.indices[i + 1]].normal;
				mesh_triangles[tri_idx].n2 = mesh_params.vertices[mesh_params.indices[i + 2]].normal;
			}

			render_mesh_t mesh = {};

			// Keep the BVH local bounds around for creating BVH instances later when building the TLAS
			bvh_node_t* bvh_root_node = (bvh_node_t*)mesh_bvh.data;
			mesh.bvh_min = bvh_root_node->aabb_min;
			mesh.bvh_max = bvh_root_node->aabb_max;

			// Upload mesh BVH buffer
			uint64_t buffer_byte_size = sizeof(bvh_header_t) + mesh_bvh_byte_size;
			uint64_t upload_byte_count = buffer_byte_size;
			uint64_t upload_offset = 0;
			bool upload_header = true;

			// Create BVH buffer
			mesh.bvh_buffer = d3d12::create_buffer(ARENA_WPRINTF(arena_scratch, L"Mesh BVH Buffer %ls", mesh_params.debug_name).buf, buffer_byte_size);

			// Allocate and initialize bvh buffer descriptor
			mesh.bvh_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			d3d12::create_buffer_srv(mesh.bvh_buffer, mesh.bvh_srv, 0, buffer_byte_size);

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
				upload.d3d_command_list->CopyBufferRegion(mesh.bvh_buffer, upload_offset, upload.d3d_resource, upload.ring_buffer_alloc.byte_offset, upload.ring_buffer_alloc.byte_size);

				// Submit upload
				d3d12::end_upload(upload);

				upload_byte_count -= upload.ring_buffer_alloc.byte_size;
				upload_offset += upload.ring_buffer_alloc.byte_size;
			}

			// Upload mesh triangle buffer
			buffer_byte_size = mesh_triangles_byte_size;
			upload_byte_count = buffer_byte_size;
			upload_offset = 0;

			// Create triangle buffer
			mesh.triangle_buffer = d3d12::create_buffer(ARENA_WPRINTF(arena_scratch, L"Mesh Triangle Buffer %ls", mesh_params.debug_name).buf, buffer_byte_size);

			// Allocate and initialize triangle buffer descriptor
			mesh.triangle_srv = d3d12::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			d3d12::create_buffer_srv(mesh.triangle_buffer, mesh.triangle_srv, 0, buffer_byte_size);

			// Do actual data upload
			while (upload_byte_count > 0)
			{
				// CPU to upload copy
				d3d12::upload_alloc_t& upload = d3d12::begin_upload(upload_byte_count);
				memcpy(upload.ptr, PTR_OFFSET(mesh_triangles, upload_offset), upload.ring_buffer_alloc.byte_size);

				// Copy current chunk from upload to final GPU buffer
				upload.d3d_command_list->CopyBufferRegion(mesh.triangle_buffer, upload_offset, upload.d3d_resource, upload.ring_buffer_alloc.byte_offset, upload.ring_buffer_alloc.byte_size);

				// Submit upload
				d3d12::end_upload(upload);

				upload_byte_count -= upload.ring_buffer_alloc.byte_size;
				upload_offset += upload.ring_buffer_alloc.byte_size;
			}

			handle = slotmap::add(g_renderer->mesh_slotmap, mesh);
		}

		return handle;
	}

	void submit_render_mesh(render_mesh_handle_t render_mesh_handle, const glm::mat4& transform, const material_t& material)
	{
		const render_mesh_t* mesh = slotmap::find(g_renderer->mesh_slotmap, render_mesh_handle);

		ASSERT_MSG(mesh, "Mesh with render mesh handle { index: %u, version: %u } was not valid", render_mesh_handle.index, render_mesh_handle.version);
		ASSERT_MSG(g_renderer->bvh_instances_at < g_renderer->bvh_instances_count, "Exceeded maximum amount of BVH instances");

		bvh_instance_t* instance = &g_renderer->bvh_instances[g_renderer->bvh_instances_at];
		instance->world_to_local = glm::inverse(transform);
		instance->bvh_index = mesh->bvh_srv.offset;

		for (uint32_t i = 0; i < 8; ++i)
		{
			glm::vec3 pos_world = transform *
				glm::vec4(i & 1 ? mesh->bvh_max.x : mesh->bvh_min.x, i & 2 ? mesh->bvh_max.y : mesh->bvh_min.y, i & 4 ? mesh->bvh_max.z : mesh->bvh_min.z, 1.0f);
			as_util::grow_aabb(instance->aabb_min, instance->aabb_max, pos_world);
		}

		// Write instance to instance buffer
		g_renderer->instance_data[g_renderer->bvh_instances_at].local_to_world = transform;
		g_renderer->instance_data[g_renderer->bvh_instances_at].world_to_local = instance->world_to_local;
		g_renderer->instance_data[g_renderer->bvh_instances_at].material = material;

		g_renderer->bvh_instances_at++;
	}

}
