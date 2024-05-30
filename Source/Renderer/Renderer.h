#pragma once

namespace Renderer
{

	enum RenderDataVisualization
	{
		RenderDataVisualization_None,
		
		RenderDataVisualization_HitNormal,

		RenderDataVisualization_Depth,

		RenderDataVisualization_RayCount,

		RenderDataVisualization_Count
	};

	static constexpr std::array<std::string, RenderDataVisualization_Count> RenderDataVisualizationLabels =
	{
		"None",
		"Hit Normal",
		"Depth",
		"Ray count"
	};

}
