#pragma once

#include "core/camera/camera.h"
#include "acceleration_structure/tlas.h"
#include "resource_slotmap.h"

struct material_t;
class bvh_t;
class bvh_instance_t;

namespace renderer
{

	enum RenderViewMode : u32
	{
		RenderViewMode_None,
		
		RenderViewMode_HitAlbedo,
		RenderViewMode_HitNormal,
		RenderViewMode_HitBarycentrics,
		RenderViewMode_HitSpecRefract,
		RenderViewMode_HitAbsorption,
		RenderViewMode_HitEmissive,

		RenderViewMode_Depth,

		RenderViewMode_RayRecursionDepth,
		RenderViewMode_RussianRouletteKillDepth,
		RenderViewMode_AccelerationStructureDepth,

		RenderViewMode_Count
	};

	static const char* render_view_mode_labels[RenderViewMode_Count] =
	{
		"None",
		"Hit albedo", "Hit normal", "Hit barycentrics", "Hit spec refract", "Hit absorption", "Hit emissive_color",
		"Depth",
		"ray_t recursion depth", "Russian roulette kill depth", "Acceleration structure depth"
	};

	struct render_settings_t
	{
		RenderViewMode render_view_mode;
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

	struct texture_t
	{
		u32 width;
		u32 height;
		f32* ptr_pixel_data;
	};

	struct renderer_inst_t
	{
		memory_arena_t arena;

		u32 render_width;
		u32 render_height;
		f32 inv_render_width;
		f32 inv_render_height;

		resource_slotmap_t<render_texture_handle_t, texture_t> texture_slotmap;
		resource_slotmap_t<render_mesh_handle_t, bvh_t> bvh_slotmap;

		u32 bvh_instances_count;
		u32 bvh_instances_at;
		bvh_instance_t* bvh_instances;
		material_t* bvh_instances_materials;

		tlas_t scene_tlas;
		camera_t scene_camera;
		texture_t* scene_hdr_env_texture;

		render_settings_t settings;
		u64 frame_index;
	};

	extern renderer_inst_t* g_renderer;

}
