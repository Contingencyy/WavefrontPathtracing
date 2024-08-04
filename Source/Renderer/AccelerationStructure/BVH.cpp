#include "Pch.h"
#include "BVH.h"
#include "Renderer/RaytracingUtils.h"
#include "Renderer/RendererFwd.h"

void BVH::Build(MemoryArena* Arena, Vertex* Vertices, u32 VertexCount, u32* Indices, u32 IndexCount, const BuildOptions& Options)
{
	m_BuildOptions = Options;
	
	m_TriangleCount = IndexCount / 3;
	m_Triangles = ARENA_ALLOC_ARRAY_ZERO(Arena, Triangle, m_TriangleCount);
	m_TriangleIndices = ARENA_ALLOC_ARRAY_ZERO(Arena, u32, m_TriangleCount);
	m_TriangleCentroids = ARENA_ALLOC_ARRAY_ZERO(Arena, glm::vec3, m_TriangleCount);

	m_BvhNodeCount = m_TriangleCount * 2;
	m_BvhNodes = ARENA_ALLOC_ARRAY_ZERO(Arena, BVHNode, m_BvhNodeCount);
	m_BvhNodeAt = 0;

	// Make BVH triangles from vertices and indices
	for (u32 TriIdx = 0, i = 0; TriIdx < m_TriangleCount; ++TriIdx, i += 3)
	{
		m_Triangles[TriIdx].p0 = Vertices[Indices[i]].Position;
		m_Triangles[TriIdx].p1 = Vertices[Indices[i + 1]].Position;
		m_Triangles[TriIdx].p2 = Vertices[Indices[i + 2]].Position;

		m_Triangles[TriIdx].n0 = Vertices[Indices[i]].Normal;
		m_Triangles[TriIdx].n1 = Vertices[Indices[i + 1]].Normal;
		m_Triangles[TriIdx].n2 = Vertices[Indices[i + 2]].Normal;
	}

	// Fill all triangle indices with their default value
	for (u32 i = 0; i < m_TriangleCount; ++i)
	{
		m_TriangleIndices[i] = i;
	}

	// Calculate all triangle centroids
	for (size_t i = 0; i < m_TriangleCount; ++i)
	{
		m_TriangleCentroids[i] = RTUtil::GetTriangleCentroid(m_Triangles[i]);
	}

	// Set the first node to be the root node
	BVHNode& RootNode = m_BvhNodes[m_BvhNodeAt];
	RootNode.LeftFirst = 0;
	RootNode.PrimCount = m_TriangleCount;

	// Skip over m_BVHNodes[1] for cache alignment
	m_BvhNodeAt = 2;

	glm::vec3 CentroidMin, CentroidMax;
	CalculateNodeMinMax(RootNode, CentroidMin, CentroidMax);
	SubdivideNode(Arena, RootNode, CentroidMin, CentroidMax, 0);
}

const Triangle* BVH::TraceRay(Ray& Ray, HitResult& HitResult) const
{
	const Triangle* HitTri = nullptr;

	const BVHNode* BvhNode = &m_BvhNodes[0];
	const BVHNode* Stack[64] = {};
	u32 StackAt = 0;

	while (true)
	{
		// The current node is a leaf node, so we need to check intersections with the primitives
		if (BvhNode->PrimCount > 0)
		{
			for (u32 TriIdx = BvhNode->LeftFirst; TriIdx < BvhNode->LeftFirst + BvhNode->PrimCount; ++TriIdx)
			{
				const Triangle& Tri = m_Triangles[m_TriangleIndices[TriIdx]];
				b8 bIntersected = RTUtil::Intersect(Tri, Ray, HitResult.t, HitResult.Bary);

				if (bIntersected)
				{
					HitResult.PrimIdx = TriIdx;
					HitTri = &m_Triangles[m_TriangleIndices[TriIdx]];
				}
			}

			if (StackAt == 0)
				break;
			else
				BvhNode = Stack[--StackAt];
			continue;
		}

		// If the current node is not a leaf node, keep traversing the left and right child nodes
		const BVHNode* LeftChildNode = &m_BvhNodes[BvhNode->LeftFirst];
		const BVHNode* RightChildNode = &m_BvhNodes[BvhNode->LeftFirst + 1];

		// Determine distance to left and right child nodes, using SSE AABB intersections
		f32 LeftDistance = RTUtil::IntersectSSE(LeftChildNode->AabbMin4, LeftChildNode->AabbMax4, Ray);
		f32 RightDistance = RTUtil::IntersectSSE(RightChildNode->AabbMin4, RightChildNode->AabbMax4, Ray);

		// Swap left and right child nodes so the closest is now the left child
		if (LeftDistance > RightDistance)
		{
			std::swap(LeftDistance, RightDistance);
			std::swap(LeftChildNode, RightChildNode);
		}

		// If we have not intersected with the left and right child nodes, we can keep traversing the stack, or terminate if stack is empty
		if (LeftDistance == FLT_MAX)
		{
			if (StackAt == 0)
				break;
			else
				BvhNode = Stack[--StackAt];
		}
		// We have intersected with either the left or right child node, so check the closest node first
		// and push the other one on the stack, if we have hit that one as well
		else
		{
			Ray.BvhDepth++;

			BvhNode = LeftChildNode;
			if (RightDistance != FLT_MAX)
				Stack[StackAt++] = RightChildNode;
		}
	}

	return HitTri;
}

AABB BVH::GetLocalSpaceAABB() const
{
	return { m_BvhNodes[0].AabbMin, m_BvhNodes[0].AabbMax };
}

void BVH::CalculateNodeMinMax(BVHNode& BvhNode, glm::vec3& CentroidMin, glm::vec3& CentroidMax)
{
	BvhNode.AabbMin = glm::vec3(FLT_MAX);
	BvhNode.AabbMax = glm::vec3(-FLT_MAX);
	CentroidMin = glm::vec3(FLT_MAX);
	CentroidMax = glm::vec3(-FLT_MAX);

	for (u32 TriIdx = BvhNode.LeftFirst; TriIdx < BvhNode.LeftFirst + BvhNode.PrimCount; ++TriIdx)
	{
		const Triangle& Tri = m_Triangles[m_TriangleIndices[TriIdx]];

		glm::vec3 TriMin, TriMax;
		RTUtil::GetTriangleMinMax(Tri, TriMin, TriMax);

		BvhNode.AabbMin = glm::min(BvhNode.AabbMin, TriMin);
		BvhNode.AabbMax = glm::max(BvhNode.AabbMax, TriMax);

		const glm::vec3& Centroid = m_TriangleCentroids[m_TriangleIndices[TriIdx]];

		CentroidMin = glm::min(CentroidMin, Centroid);
		CentroidMax = glm::max(CentroidMax, Centroid);
	}
}

f32 BVH::CalculateNodeCost(const BVHNode& BvhNode) const
{
	return BvhNode.PrimCount * RTUtil::GetAABBVolume(BvhNode.AabbMin, BvhNode.AabbMax);
}

void BVH::SubdivideNode(MemoryArena* Arena, BVHNode& BvhNode, glm::vec3& CentroidMin, glm::vec3& CentroidMax, u32 Depth)
{
	u32 SplitAxis = 0;
	u32 SplitPos = 0.0f;
	f32 SplitCost = FindBestSplitPlane(Arena, BvhNode, CentroidMin, CentroidMax, SplitAxis, SplitPos);

	// If bSubdivideToSinglePrim is enabled in the build options, we always reduce the BVH down to a single primitive per leaf-node
	if (m_BuildOptions.bSubdivideToSinglePrim)
	{
		if (BvhNode.PrimCount == 1)
			return;
	}
	// Otherwise we compare the cost of doing the split to the parent node cost, and terminate if a split is not worth it
	else
	{
		f32 ParentCost = CalculateNodeCost(BvhNode);
		if (SplitCost >= ParentCost)
			return;
	}

	// Indices to the first and last triangle indices in this node
	i32 i = BvhNode.LeftFirst;
	i32 j = i + BvhNode.PrimCount - 1;
	f32 BinScale = m_BuildOptions.IntervalCount / (CentroidMax[SplitAxis] - CentroidMin[SplitAxis]);

	// This will sort the triangles along the axis and split Position
	while (i <= j)
	{
		const glm::vec3& Centroid = m_TriangleCentroids[m_TriangleIndices[i]];

		i32 binIndex = glm::min((i32)m_BuildOptions.IntervalCount - 1,
			(i32)((Centroid[SplitAxis] - CentroidMin[SplitAxis]) * BinScale));

		if (binIndex < SplitPos)
			i++;
		else
			std::swap(m_TriangleIndices[i], m_TriangleIndices[j--]);
	}

	// Determine how many nodes are on the left side of the split axis and Position
	u32 PrimCountLeft = i - BvhNode.LeftFirst;
	// If there is no or all primitives on the left side, we can stop splitting entirely
	if (PrimCountLeft == 0 || PrimCountLeft == BvhNode.PrimCount)
		return;

	// Create two child nodes (left & right), and set their triangle start indices and count
	u32 LeftChildNodeIdx = m_BvhNodeAt++;
	BVHNode& LeftChildNode = m_BvhNodes[LeftChildNodeIdx];
	LeftChildNode.LeftFirst = BvhNode.LeftFirst;
	LeftChildNode.PrimCount = PrimCountLeft;

	u32 RightChildNodeIdx = m_BvhNodeAt++;
	BVHNode& RightChildNode = m_BvhNodes[RightChildNodeIdx];
	RightChildNode.LeftFirst = i;
	RightChildNode.PrimCount = BvhNode.PrimCount - PrimCountLeft;

	// The current node we just split now becomes a parent node,
	// meaning it contains no primitives but points to the left and right nodes
	BvhNode.LeftFirst = LeftChildNodeIdx;
	BvhNode.PrimCount = 0;

	// Calculate min/max and subdivide left child node
	CalculateNodeMinMax(LeftChildNode, CentroidMin, CentroidMax);
	SubdivideNode(Arena, LeftChildNode, CentroidMin, CentroidMax, Depth + 1);

	// Calculate min/max and subdivide right child node
	CalculateNodeMinMax(RightChildNode, CentroidMin, CentroidMax);
	SubdivideNode(Arena, RightChildNode, CentroidMin, CentroidMax, Depth + 1);
}

f32 BVH::FindBestSplitPlane(MemoryArena* Arena, BVHNode& BvhNode, const glm::vec3& CentroidMin, const glm::vec3& CentroidMax, u32& OutAxis, u32& OutSplitPosition)
{
	f32 CheapestCost = FLT_MAX;

	ARENA_MEMORY_SCOPE(Arena)
	{
		for (u32 AxisIdx = 0; AxisIdx < 3; ++AxisIdx)
		{
			f32 MinBounds = CentroidMin[AxisIdx], MaxBounds = CentroidMax[AxisIdx];
			if (MinBounds == MaxBounds)
				continue;

			u32 BinCount = m_BuildOptions.IntervalCount;
			BVHBin* BvhBins = ARENA_ALLOC_ARRAY_ZERO(Arena, BVHBin, BinCount);

			for (u32 i = 0; i < BinCount; ++i)
			{
				BVHBin* BvhBin = &BvhBins[i];

				BvhBin->AabbMin = glm::vec3(FLT_MAX);
				BvhBin->AabbMax = glm::vec3(-FLT_MAX);
			}

			f32 BinScale = BinCount / (MaxBounds - MinBounds);

			// Grow the bins
			for (u32 TriIdx = BvhNode.LeftFirst; TriIdx < BvhNode.LeftFirst + BvhNode.PrimCount; ++TriIdx)
			{
				const Triangle& Tri = m_Triangles[m_TriangleIndices[TriIdx]];
				const glm::vec3& Centroid = m_TriangleCentroids[m_TriangleIndices[TriIdx]];

				i32 BinIdx = glm::min((i32)BinCount - 1,
					(i32)((Centroid[AxisIdx] - MinBounds) * BinScale));

				BVHBin* BvhBin = &BvhBins[BinIdx];

				BvhBin->PrimCount++;
				RTUtil::GrowAABB(BvhBin->AabbMin, BvhBin->AabbMax, Tri.p0);
				RTUtil::GrowAABB(BvhBin->AabbMin, BvhBin->AabbMax, Tri.p1);
				RTUtil::GrowAABB(BvhBin->AabbMin, BvhBin->AabbMax, Tri.p2);
			}

			// Get all necessary data for the planes between the bins
			f32* LeftArea = ARENA_ALLOC_ARRAY_ZERO(Arena, f32, BinCount - 1);
			f32* RightArea = ARENA_ALLOC_ARRAY_ZERO(Arena, f32, BinCount - 1);

			glm::vec3 LeftAABBMin(FLT_MAX), LeftAABBMax(-FLT_MAX);
			glm::vec3 RightAABBMin(FLT_MAX), RightAABBMax(-FLT_MAX);

			u32 LeftSum = 0, RightSum = 0;

			for (u32 BinIdx = 0; BinIdx < BinCount - 1; ++BinIdx)
			{
				const BVHBin* LeftBin = &BvhBins[BinIdx];

				LeftSum += LeftBin->PrimCount;
				RTUtil::GrowAABB(LeftAABBMin, LeftAABBMax, LeftBin->AabbMin, LeftBin->AabbMax);
				LeftArea[BinIdx] = LeftSum * RTUtil::GetAABBVolume(LeftAABBMin, LeftAABBMax);

				const BVHBin* RightBin = &BvhBins[BinCount - 1 - BinIdx];
				RightSum += RightBin->PrimCount;
				RTUtil::GrowAABB(RightAABBMin, RightAABBMax, RightBin->AabbMin, RightBin->AabbMax);
				RightArea[BinCount - 2 - BinIdx] = RightSum * RTUtil::GetAABBVolume(RightAABBMin, RightAABBMax);
			}

			// Evaluate SAH cost for each plane
			BinScale = (MaxBounds - MinBounds) / BinCount;

			for (u32 BinIdx = 0; BinIdx < BinCount - 1; ++BinIdx)
			{
				f32 PlaneCost = LeftArea[BinIdx] + RightArea[BinIdx];

				if (PlaneCost < CheapestCost)
				{
					OutAxis = AxisIdx;
					OutSplitPosition = BinIdx + 1;
					CheapestCost = PlaneCost;
				}
			}
		}
	}

	return CheapestCost;
}
