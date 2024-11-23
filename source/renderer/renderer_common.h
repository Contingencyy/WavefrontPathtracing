#pragma once

#include "core/camera/camera.h"
#include "renderer/resource_slotmap.h"
#include "renderer/acceleration_structure/tlas_builder.h"
#include "renderer/resources/ring_buffer.h"
#include "renderer/d3d12/d3d12_descriptor.h"

#define TLAS_MAX_BVH_INSTANCES 100

struct material_t;
struct bvh_instance_t;

namespace renderer
{

	enum render_view_mode : u32
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

	struct render_settings_t
	{
		render_view_mode render_view_mode;
		u32 ray_max_recursion;
		
		b8 cosine_weighted_diffuse;
		b8 russian_roulette;

		f32 hdr_env_intensity;

		struct postfx_t
		{
			f32 max_white;
			f32 exposure;
			f32 contrast;
			f32 brightness;
			f32 saturation;
			b8 linear_to_srgb;
		} postfx;
	};

	struct render_texture_t
	{
		ID3D12Resource* texture_buffer;
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

		u32 render_width;
		u32 render_height;
		f32 inv_render_width;
		f32 inv_render_height;

		resource_slotmap_t<render_texture_handle_t, render_texture_t> texture_slotmap;
		resource_slotmap_t<render_mesh_handle_t, render_mesh_t> mesh_slotmap;

		u32 bvh_instances_count;
		u32 bvh_instances_at;
		bvh_instance_t* bvh_instances;
		material_t* bvh_instances_materials;

		tlas_t scene_tlas;
		ID3D12Resource* scene_tlas_resource;
		d3d12::descriptor_allocation_t scene_tlas_srv;

		camera_t scene_camera;
		render_texture_t* scene_hdr_env_texture;

		render_settings_t settings;
		u64 frame_index;

		ID3D12PipelineState* pso;
		ID3D12RootSignature* root_signature;

		ID3D12Resource* render_target;
		d3d12::descriptor_allocation_t render_target_uav;

		ID3D12Resource* cb_view_resource;

		ring_buffer_t upload_ring_buffer;
	};
	extern renderer_inst_t* g_renderer;

}
