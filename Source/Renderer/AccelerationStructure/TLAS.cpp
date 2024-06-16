#include "Pch.h"
#include "TLAS.h"
#include "BVH.h"
#include "Renderer/RaytracingUtils.h"

void TLAS::Build(const std::vector<BVH>& blas)
{
	m_BLAS = blas;
	m_TLASNodes.resize(m_BLAS.size() * 2);
	m_CurrentNodeIndex = 2;

	m_TLASNodes[2].leftBLAS = 0;
	m_TLASNodes[2].aabbMin = glm::vec3(FLT_MAX);
	m_TLASNodes[2].aabbMax = glm::vec3(-FLT_MAX);
	m_TLASNodes[2].isLeaf = true;

	m_TLASNodes[3].leftBLAS = 1;
	m_TLASNodes[3].aabbMin = glm::vec3(FLT_MAX);
	m_TLASNodes[3].aabbMax = glm::vec3(-FLT_MAX);
	m_TLASNodes[3].isLeaf = true;
}

void TLAS::TraceRay(Ray& ray)
{
	TLASNode* tlasNode = &m_TLASNodes[0];
	TLASNode* stack[64] = {};
	uint32_t stackPtr = 0;

	while (true)
	{
		// The current node is a leaf node, so it contains a BVH which we need to trace against
		if (tlasNode->isLeaf)
		{
			uint32_t primitiveIndex = m_BLAS[tlasNode->leftBLAS].TraceRay(ray);

			if (stackPtr == 0)
				break;
			else
				tlasNode = stack[--stackPtr];
			continue;
		}

		// If the current node is not a leaf node, keep traversing the left and right child nodes
		TLASNode* leftChildNode = &m_TLASNodes[tlasNode->leftBLAS];
		TLASNode* rightChildNode = &m_TLASNodes[tlasNode->leftBLAS + 1];

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
			tlasNode = leftChildNode;
			if (rightDistance != FLT_MAX)
				stack[stackPtr++] = rightChildNode;
		}
	}
}
