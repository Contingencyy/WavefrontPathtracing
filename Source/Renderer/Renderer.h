#pragma once

namespace Renderer
{

	enum RenderDataVisualization
	{
		RenderDataVisualization_None,
		
		RenderDataVisualization_HitAlbedo,
		RenderDataVisualization_HitNormal,

		RenderDataVisualization_Depth,

		RenderDataVisualization_RayRecursionDepth,

		RenderDataVisualization_RussianRouletteKillDepth,

		RenderDataVisualization_Count
	};

	static const std::array<std::string, RenderDataVisualization_Count> RenderDataVisualizationLabels =
	{
		"None",
		"Hit albedo", "Hit Normal",
		"Depth",
		"Ray Recursion Depth",
		"Russian Roulette Kill Depth"
	};

}
