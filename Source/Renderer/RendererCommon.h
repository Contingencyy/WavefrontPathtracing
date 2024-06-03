#pragma once

namespace Renderer
{

	enum RenderDataVisualization
	{
		RenderDataVisualization_None,
		
		RenderDataVisualization_HitAlbedo,
		RenderDataVisualization_HitNormal,
		RenderDataVisualization_HitSpecRefract,
		RenderDataVisualization_HitAbsorption,
		RenderDataVisualization_HitEmissive,

		RenderDataVisualization_Depth,

		RenderDataVisualization_RayRecursionDepth,

		RenderDataVisualization_RussianRouletteKillDepth,

		RenderDataVisualization_Count
	};

	static const std::array<std::string, RenderDataVisualization_Count> RenderDataVisualizationLabels =
	{
		"None",
		"Hit albedo", "Hit normal", "Hit spec refract", "Hit absorption", "Hit emissive",
		"Depth",
		"Ray recursion depth",
		"Russian roulette kill depth"
	};

	struct RenderSettings
	{
		RenderDataVisualization renderDataVisualization = RenderDataVisualization_None;

		bool cosineWeightedDiffuseReflection = true;
		bool russianRoulette = true;

		glm::vec3 skyColor = glm::vec3(0.52f, 0.8f, 0.92f);
		float skyColorIntensity = 0.5f;

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

}
