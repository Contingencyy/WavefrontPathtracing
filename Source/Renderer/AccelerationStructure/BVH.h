#pragma once
#include "Renderer/RendererCommon.h"
#include "Renderer/RaytracingTypes.h"

class BVH
{
public:
	struct BuildOptions
	{
		uint32_t numIntervals = 8;
		bool subdivideToSinglePrimitive = false;
		// Currently evaluateAllAxes does not do anything
		bool evaluateAllAxes = false;
	};

public:
	BVH() = default;

	void Build(const std::vector<Renderer::Vertex>& vertices, const std::vector<uint32_t>& indices, const BuildOptions& buildOptions);

	uint32_t TraceRay(Ray& ray) const;
	Triangle GetTriangle(uint32_t primID) const;

private:
	struct BVHNode
	{
		union { struct { glm::vec3 aabbMin; uint32_t leftFirst; }; __m128 aabbMin4 = {}; };
		union { struct { glm::vec3 aabbMax; uint32_t numPrimitives; }; __m128 aabbMax4 = {}; };
	};

	struct BVHBin
	{
		glm::vec3 aabbMin = glm::vec3(FLT_MAX);
		glm::vec3 aabbMax = glm::vec3(-FLT_MAX);
		uint32_t numPrimitives = 0;
	};

private:
	void CalculateNodeMinMax(BVHNode& bvhNode, glm::vec3& centroidMin, glm::vec3& centroidMax);
	float CalculateNodeCost(const BVHNode& bvhNode) const;

	void SubdivideNode(BVHNode& bvhNode, glm::vec3& centroidMin, glm::vec3& centroidMax, uint32_t depth);
	float FindBestSplitPlane(BVHNode& bvhNode, const glm::vec3& centroidMin, const glm::vec3& centroidMax, uint32_t& outAxis, uint32_t& outSplitPosition);

	glm::vec3 GetTriangleCentroid(const Triangle& bvhTriangle) const;
	void GetTriangleMinMax(const Triangle& bvhTriangle, glm::vec3& outMin, glm::vec3& outMax) const;
	float GetAABBVolume(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const;

	void GrowAABB(glm::vec3& aabbMin, glm::vec3& aabbMax, const glm::vec3& pos) const;
	void GrowAABB(glm::vec3& aabbMin, glm::vec3& aabbMax, const glm::vec3& otherMin, const glm::vec3& otherMax) const;

private:
	BuildOptions m_BuildOptions = {};

	std::vector<BVHNode> m_BVHNodes;
	size_t m_CurrentNodeIndex = 0;

	std::vector<Triangle> m_Triangles;
	std::vector<uint32_t> m_TriangleIndices;
	std::vector<glm::vec3> m_TriangleCentroids;

};
