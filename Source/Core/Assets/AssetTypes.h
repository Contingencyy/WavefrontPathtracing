#pragma once
#include "Renderer/RendererCommon.h"
#include "Renderer/AccelerationStructure/BVH.h"

#include <vector>

struct MeshAsset
{
	std::vector<Renderer::Vertex> vertices;
	std::vector<uint32_t> indices;

	BVH boundingVolumeHierarchy;
};
