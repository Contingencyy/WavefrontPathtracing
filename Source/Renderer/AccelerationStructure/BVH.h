#pragma once
#include "Renderer/RendererCommon.h"
#include "Renderer/RaytracingTypes.h"

class BVH
{
public:
	enum BuildHeuristic
	{
		// BuildHeuristic_NaiveSplit will always split each node in the middle, resulting in lesser quality BVHs
		BuildHeuristic_NaiveSplit,
		// BuildHeuristic_SAH_Intervals will split each BVH node in 8 set intervals and picks the cheapest split interval, resulting in better quality BVHs
		BuildHeuristic_SAH_Intervals,
		// BuildHeuristic_SAH_Primitives will split each BVH node along each primitive's centroid and picks the cheapest split, resulting in best quality BVHs
		BuildHeuristic_SAH_Primitives
	};

public:
	BVH() = default;

	void Build(const std::vector<Renderer::Vertex>& vertices, const std::vector<uint32_t>& indices, BuildHeuristic buildHeuristic);

	uint32_t TraceRay(Ray& ray) const;
	Triangle GetTriangle(uint32_t primID) const;

private:
	struct BVHNode
	{
		union { struct { glm::vec3 aabbMin; uint32_t leftFirst; }; __m128 aabbMin4 = {}; };
		union { struct { glm::vec3 aabbMax; uint32_t numPrimitives; }; __m128 aabbMax4 = {}; };
	};

private:
	void CalculateNodeMinMax(BVHNode& bvhNode);
	void SubdivideNode(BVHNode& bvhNode, uint32_t depth);
	void SplitNodeAlongAxisAtPosition(BVHNode& bvhNode, uint32_t axis, float splitPosition, uint32_t depth);
	float EvaluateSAHCost(BVHNode& bvhNode, uint32_t axis, float splitPosition);

	glm::vec3 GetTriangleCentroid(const Triangle& bvhTriangle) const;
	void GetTriangleMinMax(const Triangle& bvhTriangle, glm::vec3& outMin, glm::vec3& outMax) const;

	float GetAABBVolume(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const;
	void GrowAABB(glm::vec3& aabbMin, glm::vec3& aabbMax, const glm::vec3& pos) const;

private:
	BuildHeuristic m_BuildHeuristic = BuildHeuristic_SAH_Primitives;

	std::vector<BVHNode> m_BVHNodes;
	size_t m_CurrentNodeIndex = 0;

	std::vector<Triangle> m_Triangles;
	std::vector<uint32_t> m_TriangleIndices;
	std::vector<glm::vec3> m_TriangleCentroids;

};
