#include "Pch.h"
#include "TLAS.h"
#include "BVHInstance.h"
#include "Renderer/RaytracingUtils.h"

void TLAS::Build(const std::vector<BVHInstance>& blas)
{
	m_BLASInstances = blas;
	m_TLASNodes.resize(m_BLASInstances.size() * 2);

	uint32_t nodeIndex[256] = {};
	uint32_t nodeIndices = static_cast<uint32_t>(blas.size());
	m_CurrentNodeIndex = 1;

	for (size_t i = 0; i < blas.size(); ++i)
	{
		const AABB& blasBB = m_BLASInstances[i].GetWorldSpaceAABB();

		nodeIndex[i] = m_CurrentNodeIndex;

		m_TLASNodes[m_CurrentNodeIndex].aabbMin = blasBB.pmin;
		m_TLASNodes[m_CurrentNodeIndex].aabbMax = blasBB.pmax;
		m_TLASNodes[m_CurrentNodeIndex].blasInstanceIdx = i;
		m_TLASNodes[m_CurrentNodeIndex].leftRight = 0;

		m_CurrentNodeIndex++;
	}

	// Apply agglomerative clustering to build the TLAS
	uint32_t A = 0;
	uint32_t B = FindBestMatch(A, std::span<uint32_t>(nodeIndex, nodeIndices));

	while (nodeIndices > 1)
	{
		uint32_t C = FindBestMatch(B, std::span<uint32_t>(nodeIndex, nodeIndices));

		// B is the best match for A, so now we check if the best match for B is also A, and if true, combine
		if (A == C)
		{
			const uint32_t nodeIndexA = nodeIndex[A];
			const uint32_t nodeIndexB = nodeIndex[B];

			const TLASNode& nodeA = m_TLASNodes[nodeIndexA];
			const TLASNode& nodeB = m_TLASNodes[nodeIndexB];
			TLASNode& newNode = m_TLASNodes[m_CurrentNodeIndex];

			newNode.leftRight = nodeIndexA + (nodeIndexB << 16);
			newNode.aabbMin = glm::min(nodeA.aabbMin, nodeB.aabbMin);
			newNode.aabbMax = glm::max(nodeA.aabbMax, nodeB.aabbMax);

			nodeIndex[A] = m_CurrentNodeIndex++;
			nodeIndex[B] = nodeIndex[nodeIndices - 1];

			B = FindBestMatch(A, std::span<uint32_t>(nodeIndex, --nodeIndices));
		}
		else
		{
			A = B;
			B = C;
		}
	}

	m_TLASNodes[0] = m_TLASNodes[nodeIndex[A]];
}

HitResult TLAS::TraceRay(Ray& ray) const
{
	HitResult hitResult = {};

	const TLASNode* tlasNode = &m_TLASNodes[0];
	// Check if we miss the TLAS entirely
	if (RTUtil::IntersectSSE(tlasNode->aabbMin4, tlasNode->aabbMax4, ray) == FLT_MAX)
		return hitResult;

	ray.bvhDepth++;
	const TLASNode* stack[64] = {};
	uint32_t stackPtr = 0;

	while (true)
	{
		// The current node is a leaf node, so it contains a BVH which we need to trace against
		if (tlasNode->IsLeafNode())
		{
			const BVHInstance& blasInstance = m_BLASInstances[tlasNode->blasInstanceIdx];
			const glm::mat4& blasWorldToLocalTransform = blasInstance.GetWorldToLocalTransform();

			Ray intersectRay = ray;
			intersectRay.origin = blasWorldToLocalTransform * glm::vec4(ray.origin, 1.0f);
			intersectRay.dir = blasWorldToLocalTransform * glm::vec4(ray.dir, 0.0f);
			intersectRay.invDir = 1.0f / intersectRay.dir;

			uint32_t primIdx = blasInstance.TraceRay(intersectRay);

			ray.t = intersectRay.t;
			ray.bvhDepth = intersectRay.bvhDepth;

			if (primIdx != PRIM_IDX_INVALID)
			{
				hitResult.primIdx = primIdx;
				hitResult.t = ray.t;
				hitResult.pos = ray.origin + ray.dir * ray.t;
				hitResult.normal = blasInstance.GetNormal(primIdx);
				hitResult.instanceIdx = tlasNode->blasInstanceIdx;
			}

			if (stackPtr == 0)
				break;
			else
				tlasNode = stack[--stackPtr];
			continue;
		}

		// If the current node is not a leaf node, keep traversing the left and right child nodes
		const TLASNode* leftChildNode = &m_TLASNodes[tlasNode->leftRight >> 16];
		const TLASNode* rightChildNode = &m_TLASNodes[tlasNode->leftRight & 0x0000FFFF];

		// Determine distance to left and right child nodes, using SSE AABB intersections
		float leftDistance = RTUtil::IntersectSSE(leftChildNode->aabbMin4, leftChildNode->aabbMax4, ray);
		float rightDistance = RTUtil::IntersectSSE(rightChildNode->aabbMin4, rightChildNode->aabbMax4, ray);

		// Swap left and right child nodes so the closest is now the left child
		if (leftDistance > rightDistance)
		{
			std::swap(leftDistance, rightDistance);
			std::swap(leftChildNode, rightChildNode);
		}

		// If we have not intersected with the left and right child nodes, we can keep traversing the stack, or terminate if stack is empty
		if (leftDistance == FLT_MAX)
		{
			if (stackPtr == 0)
				break;
			else
				tlasNode = stack[--stackPtr];
		}
		// We have intersected with either the left or right child node, so check the closest node first
		// and push the other one on the stack, if we have hit that one as well
		else
		{
			ray.bvhDepth++;

			tlasNode = leftChildNode;
			if (rightDistance != FLT_MAX)
				stack[stackPtr++] = rightChildNode;
		}
	}

	return hitResult;
}

glm::vec3 TLAS::GetNormal(uint32_t instanceIndex, uint32_t primitiveIndex) const
{
	return m_BLASInstances[instanceIndex].GetNormal(primitiveIndex);
}

uint32_t TLAS::FindBestMatch(uint32_t A, const std::span<uint32_t>& indices)
{
	float smallestArea = FLT_MAX;
	uint32_t bestFitIndex = ~0u;

	// Find the combination of node at index A and any other node that yields the smallest bounding box
	for (size_t B = 0; B < indices.size(); ++B)
	{
		if (B != A)
		{
			glm::vec3 bmin = glm::max(m_TLASNodes[indices[A]].aabbMin, m_TLASNodes[indices[B]].aabbMin);
			glm::vec3 bmax = glm::max(m_TLASNodes[indices[A]].aabbMax, m_TLASNodes[indices[B]].aabbMax);
			glm::vec3 extent = bmax - bmin;

			float area = RTUtil::GetAABBVolume(bmin, bmax);
			if (area < smallestArea)
			{
				smallestArea = area;
				bestFitIndex = B;
			}
		}
	}

	return bestFitIndex;
}

bool TLAS::TLASNode::IsLeafNode() const
{
	return leftRight == 0;
}
