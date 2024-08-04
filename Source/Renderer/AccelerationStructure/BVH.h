#pragma once
#include "Renderer/RaytracingTypes.h"

struct Vertex;

class BVH
{
public:
	struct BuildOptions
	{
		u32 IntervalCount = 8;
		b8 bSubdivideToSinglePrim = false;
	};

public:
	void Build(MemoryArena* Arena, Vertex* Vertices, u32 VertexCount, u32* Indices, u32 IndexCount, const BuildOptions& Options);

	const Triangle* TraceRay(Ray& Ray, HitResult& HitResult) const;
	AABB GetLocalSpaceAABB() const;

private:
	struct BVHNode
	{
		union { struct { glm::vec3 AabbMin; u32 LeftFirst; }; __m128 AabbMin4 = {}; };
		union { struct { glm::vec3 AabbMax; u32 PrimCount; }; __m128 AabbMax4 = {}; };
	};

	struct BVHBin
	{
		glm::vec3 AabbMin;
		glm::vec3 AabbMax;
		u32 PrimCount;
	};

private:
	void CalculateNodeMinMax(BVHNode& BvhNode, glm::vec3& CentroidMin, glm::vec3& CentroidMax);
	f32 CalculateNodeCost(const BVHNode& BvhNode) const;

	void SubdivideNode(MemoryArena* Arena, BVHNode& BvhNode, glm::vec3& CentroidMin, glm::vec3& CentroidMax, u32 Depth);
	f32 FindBestSplitPlane(MemoryArena* Arena, BVHNode& BvhNode, const glm::vec3& CentroidMin, const glm::vec3& CentroidMax, u32& OutAxis, u32& OutSplitPosition);

private:
	BuildOptions m_BuildOptions = {};

	u32 m_BvhNodeCount;
	u32 m_BvhNodeAt;
	BVHNode* m_BvhNodes;
	
	u32 m_TriangleCount;
	Triangle* m_Triangles;
	u32* m_TriangleIndices;
	glm::vec3* m_TriangleCentroids;

};
