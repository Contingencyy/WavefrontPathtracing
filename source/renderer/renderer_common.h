#pragma once

namespace renderer
{

	enum render_visualization : u32
	{
		RenderVisualization_None,
		
		RenderVisualization_HitAlbedo,
		RenderVisualization_HitNormal,
		RenderVisualization_HitBarycentrics,
		RenderVisualization_HitSpecRefract,
		RenderVisualization_HitAbsorption,
		RenderVisualization_HitEmissive,

		RenderVisualization_Depth,

		RenderVisualization_RayRecursionDepth,
		RenderVisualization_RussianRouletteKillDepth,
		RenderVisualization_AccelerationStructureDepth,

		RenderVisualization_Count
	};

	static const char* render_data_visualization_labels[RenderVisualization_Count] =
	{
		"None",
		"Hit albedo", "Hit normal", "Hit barycentrics", "Hit spec refract", "Hit absorption", "Hit emissive_color",
		"Depth",
		"ray_t recursion depth", "Russian roulette kill depth", "Acceleration structure depth"
	};

	struct render_settings_t
	{
		render_visualization render_visualization;
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

}
