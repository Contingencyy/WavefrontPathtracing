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

#include "resource_slotmap.h"

#include "imgui/imgui.h"

namespace renderer
{

	renderer_inst_t* g_renderer = nullptr;

	static render_settings_t get_default_render_settings()
	{
		render_settings_t defaults = {};
		defaults.render_view_mode = render_view_mode::none;
		defaults.ray_max_recursion = 8;

		defaults.cosine_weighted_diffuse = true;
		defaults.russian_roulette = true;

		defaults.hdr_env_intensity = 1.0f;

		defaults.postfx.max_white = 10.0f;
		defaults.postfx.exposure = 1.0f;
		defaults.postfx.contrast = 1.0f;
		defaults.postfx.brightness = 0.0f;
		defaults.postfx.saturation = 1.0f;
		defaults.postfx.linear_to_srgb = true;

		return defaults;
	}

	void init(u32 render_width, u32 render_height)
	{
		LOG_INFO("Renderer", "Init");

		g_renderer = ARENA_BOOTSTRAP(renderer_inst_t, 0);

		g_renderer->render_width = render_width;
		g_renderer->render_height = render_height;
		g_renderer->inv_render_width = 1.0f / static_cast<f32>(g_renderer->render_width);
		g_renderer->inv_render_height = 1.0f / static_cast<f32>(g_renderer->render_height);

		g_renderer->texture_slotmap.init(&g_renderer->arena);
		g_renderer->mesh_slotmap.init(&g_renderer->arena);

		// The d3d12 backend use the same arena as the front-end renderer since the renderer initializes the backend and they have the same lifetimes
		d3d12::init(&g_renderer->arena);

		g_renderer->bvh_instances_count = TLAS_MAX_BVH_INSTANCES;
		g_renderer->bvh_instances_at = 0;
		g_renderer->bvh_instances = ARENA_ALLOC_ARRAY_ZERO(&g_renderer->arena, bvh_instance_t, g_renderer->bvh_instances_count);

		// Instance buffer
		g_renderer->instance_buffer_resource = d3d12::create_buffer_upload(L"Instance Buffer", sizeof(instance_data_t) * TLAS_MAX_BVH_INSTANCES);
		g_renderer->instance_buffer_srv = d3d12::descriptor::alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		d3d12::create_buffer_srv(g_renderer->instance_buffer_resource, g_renderer->instance_buffer_srv, 0, sizeof(instance_data_t) * TLAS_MAX_BVH_INSTANCES);
		g_renderer->instance_buffer_resource->Map(0, nullptr, reinterpret_cast<void**>(&g_renderer->instance_buffer_ptr));

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

		D3D12_ROOT_PARAMETER1 root_parameters[3] = {};
		// Global view constant buffer
		root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[0].Descriptor.ShaderRegister = 0;
		root_parameters[0].Descriptor.RegisterSpace = 0;
		root_parameters[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		// Local shader constant buffer for accessing bindless resources and other data used for that specific shader
		/*root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[1].Descriptor.ShaderRegister = 1;
		root_parameters[1].Descriptor.RegisterSpace = 0;
		root_parameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;*/

		// TODO: Remove this one, instead use a single CBV for shader specific parameters
		root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		root_parameters[1].Constants.ShaderRegister = 1;
		root_parameters[1].Constants.RegisterSpace = 0;
		root_parameters[1].Constants.Num32BitValues = 5;
		root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		root_parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		root_parameters[2].DescriptorTable.NumDescriptorRanges = ARRAY_SIZE(descriptor_ranges);
		root_parameters[2].DescriptorTable.pDescriptorRanges = descriptor_ranges;
		root_parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {};
		root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_2;
		root_signature_desc.Desc_1_2.NumParameters = ARRAY_SIZE(root_parameters);
		root_signature_desc.Desc_1_2.pParameters = root_parameters;
		root_signature_desc.Desc_1_2.NumStaticSamplers = 0;
		root_signature_desc.Desc_1_2.pStaticSamplers = nullptr;
		root_signature_desc.Desc_1_2.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

		// Root signature and PSO
		IDxcBlob* cs_binary = d3d12::compile_shader(L"shaders/pathtracer.hlsl", L"main", L"cs_6_7");
		g_renderer->root_signature = d3d12::create_root_signature(root_signature_desc);
		g_renderer->pso = d3d12::create_pso_cs(cs_binary, g_renderer->root_signature);

		// Render target
		g_renderer->render_target = d3d12::create_texture_2d(L"Final Color Texture", DXGI_FORMAT_R8G8B8A8_UNORM,
			g_renderer->render_width, g_renderer->render_height, 1,
			D3D12_RESOURCE_STATE_COMMON, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		g_renderer->render_target_uav = d3d12::descriptor::alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		d3d12::create_texture_2d_uav(g_renderer->render_target, g_renderer->render_target_uav, 0);

		// Global view constant buffer
		g_renderer->cb_view_resource = d3d12::create_buffer_upload(L"View Constant Buffer", sizeof(view_shader_data_t));

		// Initialize upload ring buffer
		ring_buffer::init(g_renderer->upload_ring_buffer, L"Upload Ring Buffer", MB(64));

		// Sanity checks
		ASSERT_MSG(render_view_mode::count == ARRAY_SIZE(render_view_mode_labels), "Not enough or too many render view mode labels defined");
	}

	void exit()
	{
		g_renderer->texture_slotmap.destroy();
		g_renderer->mesh_slotmap.destroy();

		// Wait for all potential in-flight frames to finish operations on the GPU
		d3d12::wait_on_fence(d3d12::g_d3d->sync.fence, d3d12::g_d3d->sync.fence_value);

		DX_RELEASE_OBJECT(g_renderer->root_signature);
		DX_RELEASE_OBJECT(g_renderer->pso);
		DX_RELEASE_OBJECT(g_renderer->cb_view_resource);
		g_renderer->instance_buffer_resource->Unmap(0, nullptr);
		DX_RELEASE_OBJECT(g_renderer->instance_buffer_resource);
		DX_RELEASE_OBJECT(g_renderer->render_target);

		// Ring buffer implementation waits on its own internal copy queue before safely releasing
		ring_buffer::destroy(g_renderer->upload_ring_buffer);

		d3d12::exit();
	}

	void begin_frame()
	{
		d3d12::begin_frame();
	}

	void end_frame()
	{
		d3d12::copy_to_back_buffer(g_renderer->render_target, g_renderer->render_width, g_renderer->render_height);

		d3d12::end_frame();
		d3d12::present();

		g_renderer->frame_index++;
	}

	void begin_scene(const camera_t& scene_camera, render_texture_handle_t env_render_texture_handle)
	{
		g_renderer->scene_camera = scene_camera;
		g_renderer->scene_hdr_env_texture = g_renderer->texture_slotmap.find(env_render_texture_handle);
		ASSERT(g_renderer->scene_hdr_env_texture);

		// Set new camera data for the view constant buffer
		view_shader_data_t* view_shader_data = nullptr;
		DX_CHECK_HR(g_renderer->cb_view_resource->Map(0, nullptr, reinterpret_cast<void**>(&view_shader_data)));

		glm::mat4 proj_mat = glm::perspectiveFovLH_ZO(glm::radians(scene_camera.vfov_deg),
			static_cast<float>(g_renderer->render_width), static_cast<float>(g_renderer->render_height), 0.001f, 10000.0f);

		view_shader_data->world_to_view = scene_camera.view_mat;
		view_shader_data->view_to_world = glm::inverse(scene_camera.view_mat);
		view_shader_data->view_to_clip = proj_mat;
		view_shader_data->clip_to_view = glm::inverse(proj_mat);
		view_shader_data->render_dim.x = g_renderer->render_width;
		view_shader_data->render_dim.y = g_renderer->render_height;

		g_renderer->cb_view_resource->Unmap(0, nullptr);
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
				u64 tlas_byte_size = 0;
				tlas_builder.extract(arena_scratch, g_renderer->scene_tlas, tlas_byte_size);

				// Upload TLAS to the GPU
				// Copy TLAS data from CPU to upload buffer allocation
				upload_alloc_t* upload = ring_buffer::alloc(g_renderer->upload_ring_buffer,
					sizeof(tlas_header_t) + tlas_byte_size, alignof(tlas_t));

				memcpy(upload->ptr, &g_renderer->scene_tlas.header, sizeof(tlas_header_t));
				memcpy(PTR_OFFSET(upload->ptr, sizeof(tlas_header_t)), g_renderer->scene_tlas.data, tlas_byte_size);

				// Create/resize TLAS buffer if required
				if (!g_renderer->scene_tlas_resource || g_renderer->scene_tlas_resource->GetDesc().Width < sizeof(bvh_header_t) + tlas_byte_size)
				{
					// Release old TLAS resource and create a replacement with the right size
					DX_RELEASE_OBJECT(g_renderer->scene_tlas_resource);
					g_renderer->scene_tlas_resource = d3d12::create_buffer(L"Scene TLAS Buffer", sizeof(bvh_header_t) + tlas_byte_size);
					LOG_VERBOSE("Renderer", "Created Scene TLAS Buffer at frame %u with byte size %llu", g_renderer->frame_index, sizeof(bvh_header_t) + tlas_byte_size);

					// Allocate SRV for the scene TLAS, if there is none yet
					if (!d3d12::descriptor::is_valid(g_renderer->scene_tlas_srv))
					{
						g_renderer->scene_tlas_srv = d3d12::descriptor::alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						LOG_VERBOSE("Renderer", "Allocated Scene TLAS Buffer descriptor at frame %u", g_renderer->frame_index);
					}
					// Update the scene TLAS descriptor
					d3d12::create_buffer_srv(g_renderer->scene_tlas_resource, g_renderer->scene_tlas_srv, 0, sizeof(bvh_header_t) + tlas_byte_size);
					LOG_VERBOSE("Renderer", "Updated Scene TLAS Buffer descriptor at frame %u", g_renderer->frame_index);
				}

				// Copy TLAS data from upload buffer to final buffer
				upload->d3d_command_list->CopyBufferRegion(g_renderer->scene_tlas_resource, 0,
					upload->d3d_resource, upload->byte_offset, sizeof(tlas_header_t) + tlas_byte_size);
				u64 tlas_upload_fence_value = ring_buffer::submit(g_renderer->upload_ring_buffer, upload);

				// Wait on upload buffer copy queue to finish TLAS upload before rendering
				// TODO: Have an upload ring buffer for per-frame data, so that it can run decoupled from the async asset upload ring buffer
				d3d12::g_d3d->command_queue_direct->Wait(g_renderer->upload_ring_buffer.d3d_fence, tlas_upload_fence_value);
			}
		}

		// Set descriptor heap, needs to happen before clearing UAVs
		ID3D12DescriptorHeap* descriptor_heaps[1] = { d3d12::g_d3d->descriptor_heaps.cbv_srv_uav };
		frame_ctx.command_list->SetDescriptorHeaps(ARRAY_SIZE(descriptor_heaps), descriptor_heaps);

		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			barrier.Transition.pResource = g_renderer->render_target;
			barrier.Transition.Subresource = 0;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

			frame_ctx.command_list->ResourceBarrier(1, &barrier);
		}

#if 0
		// Clear render target
		float clear_value[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		// TODO: For clearing a UAV we need a handle to a shader visible heap and a non-shader visible heap.
		// I think it might be a good idea to simply have a scratch non-shader visible heap, and whenever we need to clear a UAV,
		// we simply allocate from that scratch heap and make a UAV on the spot before clearing, or only allocate one for textures used as render targets.
		frame_ctx.d3d12_command_list->ClearUnorderedAccessViewFloat(g_renderer->render_target_uav.gpu,
			g_renderer->render_target_uav.cpu, g_renderer->render_target, clear_value, 0, nullptr);
#endif

		// Dispatch
		// TODO: Define these in a shared hlsl/cpp header, or set these as a define when compiling the shaders/PSO
		const u32 dispatch_threads_per_block_x = 16;
		const u32 dispatch_threads_per_block_y = 16;

		u32 dispatch_blocks_x = MAX((g_renderer->render_width + dispatch_threads_per_block_x - 1) / dispatch_threads_per_block_x, 1);
		u32 dispatch_blocks_y = MAX((g_renderer->render_height + dispatch_threads_per_block_y - 1) / dispatch_threads_per_block_y, 1);

		frame_ctx.command_list->SetComputeRootSignature(g_renderer->root_signature);
		frame_ctx.command_list->SetPipelineState(g_renderer->pso);

		// Bind global view constant buffer
		frame_ctx.command_list->SetComputeRootConstantBufferView(0, g_renderer->cb_view_resource->GetGPUVirtualAddress());

		// Bind shader specific constant buffer with bindless resource indices and other data related to the specific shader
		//frame_ctx.command_list->SetComputeRootConstantBufferView(1, );
		frame_ctx.command_list->SetComputeRoot32BitConstant(1, g_renderer->scene_tlas_srv.offset, 0);
		frame_ctx.command_list->SetComputeRoot32BitConstant(1, g_renderer->render_target_uav.offset, 1);
		frame_ctx.command_list->SetComputeRoot32BitConstant(1, g_renderer->scene_hdr_env_texture->texture_srv.offset, 2);
		frame_ctx.command_list->SetComputeRoot32BitConstant(1, g_renderer->instance_buffer_srv.offset, 3);
		u32 random_seed = random::rand_u32();
		frame_ctx.command_list->SetComputeRoot32BitConstant(1, random_seed, 4);

		frame_ctx.command_list->Dispatch(dispatch_blocks_x, dispatch_blocks_y, 1);
	}

	void end_scene()
	{
		g_renderer->bvh_instances_at = 0;
	}

	void render_ui()
	{
		if (ImGui::Begin("Renderer"))
		{
			b8 should_reset_accumulators = false;

			//ImGui::Text("render time: %.3f ms", Profiler::GetTimerResult(GlobalProfilingScope_RenderTime).lastElapsed * 1000.0f);

			ImGui::Text("Resolution: %ux%u", g_renderer->render_width, g_renderer->render_height);

			// Debug category
			if (ImGui::CollapsingHeader("Debug"))
			{
				ImGui::Indent(10.0f);

				// Render data visualization
				ImGui::Text("Render data visualization mode");
				if (ImGui::BeginCombo("##Render data visualization mode", render_view_mode_labels[g_renderer->settings.render_view_mode], ImGuiComboFlags_None))
				{
					for (u32 i = 0; i < count; ++i)
					{
						b8 selected = i == g_renderer->settings.render_view_mode;

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

				if (ImGui::SliderInt("Max ray Recursion Depth: %u", reinterpret_cast<i32*>(&g_renderer->settings.ray_max_recursion), 0, 8)) should_reset_accumulators = true;

				// Enable/disable cosine weighted diffuse reflections, russian roulette
				if (ImGui::Checkbox("Cosine weighted diffuse", &g_renderer->settings.cosine_weighted_diffuse)) should_reset_accumulators = true;
				if (ImGui::Checkbox("Russian roulette", &g_renderer->settings.russian_roulette)) should_reset_accumulators = true;

				// HDR environment emissive_intensity
				if (ImGui::DragFloat("HDR env emissive_intensity", &g_renderer->settings.hdr_env_intensity, 0.001f)) should_reset_accumulators = true;

				// Post-process Settings, constract, brightness, saturation, sRGB
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
				if (ImGui::CollapsingHeader("Post-processing"))
				{
					ImGui::Indent(10.0f);

					if (ImGui::SliderFloat("Max white", &g_renderer->settings.postfx.max_white, 0.0f, 100.0f)) should_reset_accumulators = true;
					if (ImGui::SliderFloat("Exposure", &g_renderer->settings.postfx.exposure, 0.0f, 100.0f)) should_reset_accumulators = true;
					if (ImGui::SliderFloat("Contrast", &g_renderer->settings.postfx.contrast, 0.0f, 3.0f)) should_reset_accumulators = true;
					if (ImGui::SliderFloat("Brightness", &g_renderer->settings.postfx.brightness, -1.0f, 1.0f)) should_reset_accumulators = true;
					if (ImGui::SliderFloat("Saturation", &g_renderer->settings.postfx.saturation, 0.0f, 10.0f)) should_reset_accumulators = true;
					if (ImGui::Checkbox("Linear to SRGB", &g_renderer->settings.postfx.linear_to_srgb)) should_reset_accumulators = true;

					ImGui::Unindent(10.0f);
				}

				ImGui::Unindent(10.0f);
			}
		}

		ImGui::End();
	}

	render_texture_handle_t create_render_texture(const render_texture_params_t& texture_params)
	{
		// TODO: Support different DXGI formats, figure out format based on texture_params
		// TODO: Support other texture dimensions than 2D
		// TODO: Support uploading textures with mips
		
		ID3D12Resource* d3d_resource = nullptr;
		ARENA_SCRATCH_SCOPE()
		{
			d3d_resource = d3d12::create_texture_2d(ARENA_WPRINTF(arena_scratch, L"Texture Buffer %ls", texture_params.debug_name).buf,
				DXGI_FORMAT_R32G32B32A32_FLOAT, texture_params.width, texture_params.height, 1, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, D3D12_RESOURCE_FLAG_NONE);
		}
		u32 src_rowpitch = texture_params.width * texture_params.bytes_per_channel * texture_params.channel_count;

		// Prepare texture copy destination
		D3D12_RESOURCE_DESC dst_desc = d3d_resource->GetDesc();
		D3D12_SUBRESOURCE_FOOTPRINT dst_footprint = {};
		dst_footprint.Format = dst_desc.Format;
		dst_footprint.Width = dst_desc.Width;
		dst_footprint.Height = dst_desc.Height;
		dst_footprint.Depth = dst_desc.DepthOrArraySize;
		dst_footprint.RowPitch = ALIGN_UP_POW2(src_rowpitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_TEXTURE_COPY_LOCATION dst_loc = {};
		dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_loc.pResource = d3d_resource;
		dst_loc.SubresourceIndex = 0;

		// Upload in chunks of rows if the upload ring buffer cannot accomodate the entire texture at once
		u32 rows_to_copy = dst_desc.Height;
		u32 row_curr = 0;

		while (rows_to_copy > 0)
		{
			// Allocate a chunk of CPU writable memory from the upload ring buffer, with a minimum required size of the destination row pitch
			u64 required_bytes = rows_to_copy * dst_footprint.RowPitch;
			upload_alloc_t* upload = ring_buffer::alloc(g_renderer->upload_ring_buffer,
				required_bytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

			// Determine how many rows we can copy over with the upload allocation we got back, it might have returned less memory
			u32 row_count_upload = upload->byte_size / dst_footprint.RowPitch;
			ASSERT_MSG(row_count_upload, "Uploading texture failed because upload ring buffer is not large enough to upload a single row"
				"Texture: %ls, Width: %u, Height: %u, Mips: %u, Bytes per channel: %u, Channels: %u",
				texture_params.debug_name, texture_params.width, texture_params.height, 1, texture_params.bytes_per_channel, texture_params.channel_count);

			// Copy texture data to the upload allocation
			u8* ptr_src = texture_params.ptr_data + src_rowpitch * row_curr;
			u8* ptr_dst = upload->ptr;

			for (u32 y = 0; y < row_count_upload; ++y)
			{
				memcpy(ptr_dst, ptr_src, dst_footprint.RowPitch);
				ptr_src += src_rowpitch;
				ptr_dst += dst_footprint.RowPitch;
			}

			// Update the source footprint and source copy location with the current upload allocation
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT src_footprint = {};
			src_footprint.Footprint = dst_footprint;
			src_footprint.Footprint.Height = row_count_upload;
			src_footprint.Offset = upload->byte_offset;

			D3D12_TEXTURE_COPY_LOCATION src_loc = {};
			src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src_loc.pResource = upload->d3d_resource;
			src_loc.PlacedFootprint = src_footprint;

			upload->d3d_command_list->CopyTextureRegion(&dst_loc, 0, row_curr, 0, &src_loc, nullptr);
			ring_buffer::submit(g_renderer->upload_ring_buffer, upload);

			// Update rows that are left to upload
			rows_to_copy -= row_count_upload;
			row_curr += row_count_upload;
		}

		render_texture_t render_texture = {};
		render_texture.texture_buffer = d3d_resource;
		render_texture.texture_srv = d3d12::descriptor::alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		d3d12::create_texture_2d_srv(render_texture.texture_buffer, render_texture.texture_srv, 0, DXGI_FORMAT_R32G32B32A32_FLOAT);

		render_texture_handle_t handle = g_renderer->texture_slotmap.add(std::move(render_texture));
		return handle;
	}

	render_mesh_handle_t create_render_mesh(const render_mesh_params_t& mesh_params)
	{
		bvh_builder_t::build_args_t bvh_build_args = {};
		bvh_build_args.vertex_count = mesh_params.vertex_count;
		bvh_build_args.vertices = mesh_params.vertices;
		bvh_build_args.index_count = mesh_params.index_count;
		bvh_build_args.indices = mesh_params.indices;
		//bvh_build_args.options.interval_count = 8;
		//bvh_build_args.options.subdivide_single_prim = false;

		render_mesh_handle_t handle = {};

		ARENA_SCRATCH_SCOPE()
		{
			bvh_builder_t bvh_builder = {};
			bvh_builder.build(arena_scratch, bvh_build_args);

			bvh_t mesh_bvh;
			u64 mesh_bvh_byte_size;
			bvh_builder.extract(arena_scratch, mesh_bvh, mesh_bvh_byte_size);

			u32 mesh_triangle_count = mesh_params.index_count / 3;
			triangle_t* mesh_triangles = ARENA_ALLOC_ARRAY(arena_scratch, triangle_t, mesh_triangle_count);
			u64 mesh_triangles_byte_size = sizeof(triangle_t) * mesh_triangle_count;

			for (u32 tri_idx = 0, i = 0; tri_idx < mesh_triangle_count; ++tri_idx, i += 3)
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
			{
				// CPU to upload copy
				upload_alloc_t* upload = ring_buffer::alloc(g_renderer->upload_ring_buffer, sizeof(bvh_header_t) + mesh_bvh_byte_size, 4);
				// TODO: If the mesh is larger than the ring buffer, need to upload in chunks
				ASSERT(upload->byte_size >= sizeof(bvh_header_t) + mesh_bvh_byte_size);

				memcpy(upload->ptr, &mesh_bvh.header, sizeof(bvh_header_t));
				memcpy(PTR_OFFSET(upload->ptr, sizeof(bvh_header_t)), mesh_bvh.data, mesh_bvh_byte_size);

				// Create GPU buffer and do copy from upload to GPU
				mesh.bvh_buffer = d3d12::create_buffer(ARENA_WPRINTF(arena_scratch, L"Mesh BVH Buffer %ls", mesh_params.debug_name).buf, sizeof(bvh_header_t) + mesh_bvh_byte_size);
				upload->d3d_command_list->CopyBufferRegion(mesh.bvh_buffer, 0, upload->d3d_resource, upload->byte_offset, sizeof(bvh_header_t) + mesh_bvh_byte_size);

				// Allocate and initialize bvh buffer descriptor
				mesh.bvh_srv = d3d12::descriptor::alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				d3d12::create_buffer_srv(mesh.bvh_buffer, mesh.bvh_srv, 0, sizeof(bvh_header_t) + mesh_bvh_byte_size);

				// Submit upload
				ring_buffer::submit(g_renderer->upload_ring_buffer, upload);
			}

			// Upload mesh triangle buffer
			{
				// CPU to upload copy
				upload_alloc_t* upload = ring_buffer::alloc(g_renderer->upload_ring_buffer, mesh_triangles_byte_size, 4);
				// TODO: If the mesh is larger than the ring buffer, need to upload in chunks
				ASSERT(upload->byte_size >= mesh_triangles_byte_size);

				memcpy(upload->ptr, mesh_triangles, mesh_triangles_byte_size);

				// Create GPU buffer and do copy from upload to GPU
				mesh.triangle_buffer = d3d12::create_buffer(ARENA_WPRINTF(arena_scratch, L"Mesh Triangle Buffer %ls", mesh_params.debug_name).buf, mesh_triangles_byte_size);
				upload->d3d_command_list->CopyBufferRegion(mesh.triangle_buffer, 0, upload->d3d_resource, upload->byte_offset, mesh_triangles_byte_size);

				// Allocate and initialize triangle buffer descriptor
				mesh.triangle_srv = d3d12::descriptor::alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				d3d12::create_buffer_srv(mesh.triangle_buffer, mesh.triangle_srv, 0, mesh_triangles_byte_size);

				// Submit upload
				ring_buffer::submit(g_renderer->upload_ring_buffer, upload);
			}

			handle = g_renderer->mesh_slotmap.add(std::move(mesh));
		}

		return handle;
	}

	void submit_render_mesh(render_mesh_handle_t render_mesh_handle, const glm::mat4& transform, const material_t& material)
	{
		const render_mesh_t* mesh = g_renderer->mesh_slotmap.find(render_mesh_handle);

		ASSERT_MSG(mesh, "Mesh with render mesh handle { index: %u, version: %u } was not valid", render_mesh_handle.index, render_mesh_handle.version);
		ASSERT_MSG(g_renderer->bvh_instances_at < g_renderer->bvh_instances_count, "Exceeded maximum amount of BVH instances");

		bvh_instance_t* instance = &g_renderer->bvh_instances[g_renderer->bvh_instances_at];
		instance->world_to_local = glm::inverse(transform);
		instance->bvh_index = mesh->bvh_srv.offset;

		for (u32 i = 0; i < 8; ++i)
		{
			glm::vec3 pos_world = transform *
				glm::vec4(i & 1 ? mesh->bvh_max.x : mesh->bvh_min.x, i & 2 ? mesh->bvh_max.y : mesh->bvh_min.y, i & 4 ? mesh->bvh_max.z : mesh->bvh_min.z, 1.0f);
			as_util::grow_aabb(instance->aabb_min, instance->aabb_max, pos_world);
		}

		// Write instance to instance buffer
		g_renderer->instance_buffer_ptr[g_renderer->bvh_instances_at].local_to_world = transform;
		g_renderer->instance_buffer_ptr[g_renderer->bvh_instances_at].world_to_local = instance->world_to_local;
		g_renderer->instance_buffer_ptr[g_renderer->bvh_instances_at].material = material;

		g_renderer->bvh_instances_at++;
	}

}
