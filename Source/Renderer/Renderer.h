#pragma once

namespace Renderer
{

	enum RenderDataVisualization
	{
		RenderDataVisualization_None,
		
		RenderDataVisualization_HitNormal,

		RenderDataVisualization_Depth,

		RenderDataVisualization_RayRecursionDepth,

		RenderDataVisualization_Count
	};

	static const std::array<std::string, RenderDataVisualization_Count> RenderDataVisualizationLabels =
	{
		"None",
		"Hit Normal",
		"Depth",
		"Ray Recursion Depth"
	};

}
