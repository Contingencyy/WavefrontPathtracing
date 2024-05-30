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

}
