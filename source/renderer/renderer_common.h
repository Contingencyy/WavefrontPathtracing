#pragma once

#include "core/camera/camera.h"
#include "core/containers/slotmap.h"

#include "shaders/shared.hlsl.h"

#include "renderer/acceleration_structure/tlas_builder.h"

#include "renderer/d3d12/d3d12_descriptor.h"
#include "renderer/d3d12/d3d12_frame.h"

struct material_t;
struct bvh_instance_t;

namespace renderer
{

	inline constexpr uint32_t TLAS_MAX_BVH_INSTANCES = 1024;

	enum render_view_mode : uint32_t
	{
		none,

		hit_albedo,
		hit_normal,
		hit_barycentrics,
		hit_spec_refract,
		hit_absorption,
		hit_emissive,

		depth,
		ray_recursion_depth,
		russian_roulette_kill_depth,
		acceleration_struct_depth,

		count
	};

	static const char* render_view_mode_labels[render_view_mode::count] =
	{
		"None",
		"Hit albedo", "Hit normal", "Hit barycentrics", "Hit spec refract", "Hit absorption", "Hit emissive_color",
		"Depth",
		"ray_t recursion depth", "Russian roulette kill depth", "Acceleration structure depth"
	};
	static_assert(render_view_mode::count == ARRAY_SIZE(render_view_mode_labels));

	struct render_texture_t
	{
		ID3D12Resource* texture_buffer;
		d3d12::descriptor_allocation_t texture_srv;
	};

	struct render_mesh_t
	{
		ID3D12Resource* bvh_buffer;
		d3d12::descriptor_allocation_t bvh_srv;
		glm::vec3 bvh_min;
		glm::vec3 bvh_max;

		ID3D12Resource* triangle_buffer;
		d3d12::descriptor_allocation_t triangle_srv;
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

		uint32_t bvh_instances_count;
		uint32_t bvh_instances_at;
		bvh_instance_t* bvh_instances;
		instance_data_t* instance_data;

		tlas_t scene_tlas;
		ID3D12Resource* scene_tlas_resource;
		d3d12::descriptor_allocation_t scene_tlas_srv;

		ID3D12Resource* instance_buffer;
		d3d12::descriptor_allocation_t instance_buffer_srv;

		camera_t scene_camera;
		render_texture_t* scene_hdr_env_texture;

		render_settings_shader_data_t settings;
		uint64_t frame_index;

		ID3D12RootSignature* root_signature;
		ID3D12PipelineState* pso_cs_pathtracer;
		ID3D12PipelineState* pso_cs_post_process;

		ID3D12Resource* rt_color_accum;
		d3d12::descriptor_allocation_t rt_color_accum_srv_uav;
		uint32_t accum_count;
		ID3D12Resource* rt_final_color;
		d3d12::descriptor_allocation_t rt_final_color_uav;

		d3d12::frame_alloc_t cb_render_settings;
		d3d12::frame_alloc_t cb_view;
	};
	extern renderer_inst_t* g_renderer;

}
