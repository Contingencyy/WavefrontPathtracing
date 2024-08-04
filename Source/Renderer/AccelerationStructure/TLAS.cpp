#include "Pch.h"
#include "TLAS.h"
#include "BVHInstance.h"
#include "Renderer/RaytracingUtils.h"

void TLAS::Build(MemoryArena* Arena, BVHInstance* BLAS, u32 BLASCount)
{
	m_BLASInstanceCount = BLASCount;
	m_BLASInstances = BLAS;

	m_TLASNodeCount = m_BLASInstanceCount * 2;
	m_TLASNodes = ARENA_ALLOC_ARRAY_ZERO(Arena, TLASNode, m_TLASNodeCount);

	u32 NodeIdx[256] = {};
	u32 NodeIndices = BLASCount;
	m_NodeAt = 1;

	for (size_t i = 0; i < BLASCount; ++i)
	{
		const AABB& BLASBoundingBox = m_BLASInstances[i].GetWorldSpaceAABB();

		NodeIdx[i] = m_NodeAt;

		m_TLASNodes[m_NodeAt].AabbMin = BLASBoundingBox.PMin;
		m_TLASNodes[m_NodeAt].AabbMax = BLASBoundingBox.PMax;
		m_TLASNodes[m_NodeAt].BlasInstanceIdx = i;
		m_TLASNodes[m_NodeAt].LeftRight = 0;

		m_NodeAt++;
	}

	// Apply agglomerative clustering to build the TLAS
	u32 A = 0;
	u32 B = FindBestMatch(A, NodeIdx, NodeIndices);

	while (NodeIndices > 1)
	{
		u32 C = FindBestMatch(B, NodeIdx, NodeIndices);

		// B is the best match for A, so now we check if the best match for B is also A, and if true, combine
		if (A == C)
		{
			const u32 NodeIndexA = NodeIdx[A];
			const u32 NodeIndexB = NodeIdx[B];

			const TLASNode& NodeA = m_TLASNodes[NodeIndexA];
			const TLASNode& NodeB = m_TLASNodes[NodeIndexB];
			TLASNode& NewNode = m_TLASNodes[m_NodeAt];

			NewNode.LeftRight = NodeIndexA + (NodeIndexB << 16);
			NewNode.AabbMin = glm::min(NodeA.AabbMin, NodeB.AabbMin);
			NewNode.AabbMax = glm::max(NodeA.AabbMax, NodeB.AabbMax);

			NodeIdx[A] = m_NodeAt++;
			NodeIdx[B] = NodeIdx[NodeIndices - 1];

			B = FindBestMatch(A, NodeIdx, --NodeIndices);
		}
		else
		{
			A = B;
			B = C;
		}
	}

	m_TLASNodes[0] = m_TLASNodes[NodeIdx[A]];
}

void TLAS::TraceRay(Ray& Ray, HitResult& HitResult) const
{
	const TLASNode* Node = &m_TLASNodes[0];
	// Check if we miss the TLAS entirely
	if (RTUtil::IntersectSSE(Node->AabbMin4, Node->AabbMax4, Ray) == FLT_MAX)
		return;

	Ray.BvhDepth++;
	const TLASNode* Stack[64] = {};
	u32 StackAt = 0;

	while (true)
	{
		// The current node is a leaf node, so it contains a BVH which we need to trace against
		if (Node->IsLeafNode())
		{
			const BVHInstance& BLAS = m_BLASInstances[Node->BlasInstanceIdx];

			// Trace object-space ray against object-space BVH
			const Triangle* HitTri = BLAS.TraceRay(Ray, HitResult);

			// If we have hit a triangle, update the hit result
			if (HitTri)
			{
				// Hit t, Bary and PrimIdx are written in BVH::TraceRay, Hit Pos and Normal in BVHInstance::TraceRay
				HitResult.InstanceIdx = Node->BlasInstanceIdx;
			}

			if (StackAt == 0)
				break;
			else
				Node = Stack[--StackAt];
			continue;
		}

		// If the current node is not a leaf node, keep traversing the left and right child nodes
		const TLASNode* LeftChildNode = &m_TLASNodes[Node->LeftRight >> 16];
		const TLASNode* RightChildNode = &m_TLASNodes[Node->LeftRight & 0x0000FFFF];

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
				Node = Stack[--StackAt];
		}
		// We have intersected with either the left or right child node, so check the closest node first
		// and push the other one on the stack, if we have hit that one as well
		else
		{
			Ray.BvhDepth++;

			Node = LeftChildNode;
			if (RightDistance != FLT_MAX)
				Stack[StackAt++] = RightChildNode;
		}
	}
}

u32 TLAS::FindBestMatch(u32 A, const u32* Indices, u32 IndexCount)
{
	f32 SmallestArea = FLT_MAX;
	u32 BestFitIdx = ~0u;

	// Find the combination of node at Index A and any other node that yields the smallest bounding box
	for (size_t B = 0; B < IndexCount; ++B)
	{
		if (B != A)
		{
			glm::vec3 BMax = glm::max(m_TLASNodes[Indices[A]].AabbMin, m_TLASNodes[Indices[B]].AabbMin);
			glm::vec3 BMin = glm::max(m_TLASNodes[Indices[A]].AabbMax, m_TLASNodes[Indices[B]].AabbMax);
			glm::vec3 Extent = BMax - BMin;

			f32 Area = RTUtil::GetAABBVolume(BMin, BMax);
			if (Area < SmallestArea)
			{
				SmallestArea = Area;
				BestFitIdx = B;
			}
		}
	}

	return BestFitIdx;
}

b8 TLAS::TLASNode::IsLeafNode() const
{
	return LeftRight == 0;
}
