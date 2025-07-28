#pragma once

#include "core/camera/camera.h"
#include "core/containers/slotmap.h"
#include "core/containers/hashmap.h"

#include "shaders/shared.hlsl.h"

#include "renderer/renderer_fwd.h"
#include "renderer/acceleration_structure/tlas_builder.h"

#include "renderer/d3d12/d3d12_descriptor.h"
#include "renderer/d3d12/d3d12_frame.h"
#include "renderer/gpu_profiler.h"

struct material_t;
struct bvh_instance_t;

namespace renderer
{

	inline constexpr uint32_t MAX_INSTANCES = 1024;
	inline constexpr uint32_t GPU_PROFILER_MAX_HISTORY = 512;

	static const char* render_view_mode_labels[RENDER_VIEW_MODE_COUNT] =
	{
		"None",
		"Geometry Instance", "Geometry Primitive", "Geometry Barycentrics",
		"Material Base Color", "Material Metallic Roughness", "Material Emissive",
		"World Normal",
		"RenderTarget Depth"
	};
	static_assert(RENDER_VIEW_MODE_COUNT == ARRAY_SIZE(render_view_mode_labels));

	struct render_texture_t
	{
		ID3D12Resource* texture_buffer;
		d3d12::descriptor_allocation_t texture_srv;

		uint32_t width;
		uint32_t height;
		uint32_t depth;

		wstring_t debug_name;
	};

	struct render_mesh_t
	{
		glm::vec3 blas_min;
		glm::vec3 blas_max;
		ID3D12Resource* blas_buffer;
		d3d12::descriptor_allocation_t blas_srv;

		triangle_t* triangles;
		uint32_t triangle_count;
		ID3D12Resource* triangle_buffer;
		d3d12::descriptor_allocation_t triangle_srv;

		wstring_t debug_name;
	};

	struct frame_context_t
	{
		memory_arena_t arena;
		
		ID3D12Resource* scene_tlas_scratch_resource;
		ID3D12Resource* scene_tlas_resource;
		d3d12::descriptor_allocation_t scene_tlas_srv;

		gpu_timer_query_t* gpu_timer_queries;
		uint32_t gpu_timer_queries_at;
	};
	
	struct gpu_profiler_t
	{
		uint32_t history_write_offset;
		uint32_t history_read_offset;
		gpu_profile_scope_result_t profile_scope_history[GPU_PROFILER_MAX_HISTORY][GPU_PROFILE_SCOPE_COUNT];

		struct ui_t
		{
			bool paused;
			bool autofit_graph_yaxis;
			bool disabled_scopes[GPU_PROFILE_SCOPE_COUNT];
		} ui;
	};

	struct renderer_inst_t
	{
		memory_arena_t arena;

		uint32_t render_width;
		uint32_t render_height;
		float inv_render_width;
		float inv_render_height;

		slotmap_t<render_texture_handle_t, render_texture_t> texture_slotmap;
		slotmap_t<render_mesh_handle_t, render_mesh_t> mesh_slotmap;

		bool change_raytracing_mode;

		// Only used for software raytracing
		bvh_instance_t* tlas_instance_data_software;
		tlas_t scene_tlas;

		// Only used for hardware raytracing
		D3D12_RAYTRACING_INSTANCE_DESC* tlas_instance_data_hardware;
		ID3D12Resource* tlas_instance_data_buffer;

		// Instance data not used for raytracing AS builds
		uint32_t instance_data_capacity;
		uint32_t instance_data_at;
		instance_data_t* instance_data;
		ID3D12Resource* instance_buffer;
		d3d12::descriptor_allocation_t instance_buffer_srv;

		// Scene TLAS resource
		frame_context_t* frame_ctx;

		camera_t scene_camera;
		render_texture_t* scene_hdr_env_texture;

		render_settings_shader_data_t settings;
		uint64_t frame_index;

		struct defaults_t
		{
			render_texture_t* texture_base_color;
			render_texture_handle_t texture_handle_base_color;
			render_texture_t* texture_normal;
			render_texture_handle_t texture_handle_normal;
			render_texture_t* texture_metallic_roughness;
			render_texture_handle_t texture_handle_metallic_roughness;
			render_texture_t* texture_emissive;
			render_texture_handle_t texture_handle_emissive;
		} defaults;

		struct wavefront_t
		{
			ID3D12CommandSignature* command_signature;

			ID3D12PipelineState* pso_clear_buffers;
			ID3D12PipelineState* pso_init_args;
			ID3D12PipelineState* pso_generate;
			ID3D12PipelineState* pso_extend;
			ID3D12PipelineState* pso_shade;
			//ID3D12PipelineState* pso_connect;
			
			ID3D12Resource* buffer_indirect_args;
			ID3D12Resource* buffer_ray_counts;
			ID3D12Resource* buffer_ray;
			// RGBA16 float, Alpha channel is unused
			ID3D12Resource* texture_energy;
			// RGBA16 float, Alpha channel is unused
			ID3D12Resource* texture_throughput;
			ID3D12Resource* buffer_pixelpos;
			ID3D12Resource* buffer_pixelpos_two;
			ID3D12Resource* buffer_intersection;

			d3d12::descriptor_allocation_t buffer_indirect_args_srv_uav;
			d3d12::descriptor_allocation_t buffer_ray_counts_srv_uav;
			d3d12::descriptor_allocation_t buffer_ray_srv_uav;
			d3d12::descriptor_allocation_t texture_energy_srv_uav;
			d3d12::descriptor_allocation_t texture_throughput_srv_uav;
			d3d12::descriptor_allocation_t buffer_pixelpos_srv_uav;
			d3d12::descriptor_allocation_t buffer_pixelpos_two_srv_uav;
			d3d12::descriptor_allocation_t buffer_intersection_srv_uav;
		} wavefront;

		ID3D12RootSignature* root_signature;
		ID3D12PipelineState* pso_cs_pathtracer_software;
		ID3D12PipelineState* pso_cs_pathtracer_hardware;
		ID3D12PipelineState* pso_cs_post_process;

		ID3D12Resource* rt_color_accum;
		d3d12::descriptor_allocation_t rt_color_accum_srv_uav;
		uint32_t accum_count;
		ID3D12Resource* rt_final_color;
		d3d12::descriptor_allocation_t rt_final_color_uav;

		d3d12::frame_resource_t cb_render_settings;
		d3d12::frame_resource_t cb_view;

		gpu_profiler_t gpu_profiler;
	};
	extern renderer_inst_t* g_renderer;
	
	frame_context_t& get_frame_context();

}
