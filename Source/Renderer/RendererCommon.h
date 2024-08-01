#pragma once

namespace Renderer
{

	enum RenderVisualization : uint32_t
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

	static const std::array<std::string, RenderVisualization_Count> RenderDataVisualizationLabels =
	{
		"None",
		"Hit albedo", "Hit normal", "Hit barycentrics", "Hit spec refract", "Hit absorption", "Hit emissive",
		"Depth",
		"Ray recursion depth", "Russian roulette kill depth", "Acceleration structure depth"
	};

	struct RenderSettings
	{
		RenderVisualization renderVisualization = RenderVisualization_None;
		uint32_t rayMaxRecursionDepth = 8;
		
		bool cosineWeightedDiffuseReflection = true;
		bool russianRoulette = true;

		float hdrEnvIntensity = 1.0f;

		struct PostFX
		{
			float maxWhite = 10.0f;
			float exposure = 1.0f;
			float contrast = 1.0f;
			float brightness = 0.0f;
			float saturation = 1.0f;
			bool linearToSRGB = true;
		} postfx;
	};

	struct Texture
	{
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<glm::vec4> pixelData;
	};

}
