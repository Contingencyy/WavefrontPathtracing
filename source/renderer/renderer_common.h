#pragma once

#include "core/camera/camera.h"
#include "core/containers/slotmap.h"
#include "core/containers/hashmap.h"

#include "shaders/shared.hlsl.h"

#include "renderer/acceleration_structure/tlas_builder.h"

#include "renderer/d3d12/d3d12_descriptor.h"
#include "renderer/d3d12/d3d12_frame.h"

struct material_t;
struct bvh_instance_t;

namespace renderer
{

	inline constexpr uint32_t MAX_INSTANCES = 1024;

	enum render_view_mode : uint32_t
	{
		none,

		geometry_instance,
		geometry_primitive,
		geometry_barycentrics,
		geometry_normal,

		material_albedo,
		material_normal,
		material_spec_refract,
		material_absorption,
		material_emissive,

		world_normal,
		world_tangent,
		world_bitangent,

		path_depth,

		render_target_depth,

		count
	};

	static const char* render_view_mode_labels[render_view_mode::count] =
	{
		"None",
		"Geometry Instance", "Geometry Primitive", "Geometry Barycentrics", "Geometry Normal",
		"Material Albedo", "Material Normal", "Material Spec Refract", "Material Absorption", "Material Emissive",
		"World Normal", "World Tangent", "World Bitangent",
		"Path Depth",
		"RenderTarget Depth"
	};
	static_assert(render_view_mode::count == ARRAY_SIZE(render_view_mode_labels));

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

	struct gpu_timer_query_t
	{
		uint32_t query_idx_begin;
		uint32_t query_idx_end;

		ID3D12CommandQueue* d3d_command_queue;
	};

	struct gpu_timer_result_t
	{
		uint64_t timestamp_begin;
		uint64_t timestamp_end;
	};

	struct frame_context_t
	{
		ID3D12Resource* scene_tlas_scratch_resource;
		ID3D12Resource* scene_tlas_resource;
		d3d12::descriptor_allocation_t scene_tlas_srv;

		hashmap_t<string_t, gpu_timer_query_t> gpu_timer_queries;
	};

	struct renderer_inst_t
	{
		memory_arena_t arena;
		memory_arena_t frame_arena;

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

		struct wavefront_t
		{
			ID3D12PipelineState* pso_generate;
			ID3D12PipelineState* pso_extend;
			ID3D12PipelineState* pso_shade;
			//ID3D12PipelineState* pso_connect;

			ID3D12Resource* buffer_ray;
			ID3D12Resource* buffer_energy;
			ID3D12Resource* buffer_throughput;
			ID3D12Resource* buffer_pixelpos;
			ID3D12Resource* buffer_intersection;

			d3d12::descriptor_allocation_t buffer_ray_srv_uav;
			d3d12::descriptor_allocation_t buffer_energy_srv_uav;
			d3d12::descriptor_allocation_t buffer_throughput_srv_uav;
			d3d12::descriptor_allocation_t buffer_pixelpos_srv_uav;
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
		
		hashmap_t<string_t, gpu_timer_result_t> gpu_timer_results;
	};
	extern renderer_inst_t* g_renderer;

}
