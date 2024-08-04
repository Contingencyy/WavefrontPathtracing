#pragma once

namespace Renderer
{

	enum RenderVisualization : u32
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

	static const char* RenderDataVisualizationLabels[RenderVisualization_Count] =
	{
		"None",
		"Hit Albedo", "Hit Normal", "Hit barycentrics", "Hit spec refract", "Hit Absorption", "Hit Emissive",
		"Depth",
		"Ray recursion depth", "Russian roulette kill depth", "Acceleration structure depth"
	};

	struct RenderSettings
	{
		RenderVisualization RenderVisualization;
		u32 RayMaxRecursionDepth;
		
		b8 bCosineWeightedDiffuseReflection;
		b8 bRussianRoulette;

		f32 HdrEnvIntensity;

		struct PostFX
		{
			f32 MaxWhite;
			f32 Exposure;
			f32 Contrast;
			f32 Brightness;
			f32 Saturation;
			b8 bLinearToSRGB;
		} Postfx;
	};

	struct Texture
	{
		u32 Width;
		u32 Height;
		f32* PtrPixelData;
	};

}
