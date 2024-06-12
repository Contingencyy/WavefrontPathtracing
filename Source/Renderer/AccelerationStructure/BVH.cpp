#include "Pch.h"
#include "BVH.h"
#include "Renderer/RaytracingUtils.h"

void BVH::Build(const std::vector<Renderer::Vertex>& vertices, const std::vector<uint32_t>& indices, BuildHeuristic buildHeuristic)
{
	m_BuildHeuristic = buildHeuristic;

	m_Triangles.resize(indices.size() / 3);
	m_TriangleIndices.resize(m_Triangles.size());
	m_TriangleCentroids.resize(m_Triangles.size());
	m_BVHNodes.resize(m_Triangles.size() * 2 - 1);

	// Make BVH triangles from vertices and indices
	for (size_t triIndex = 0, i = 0; triIndex < m_Triangles.size(); ++triIndex, i += 3)
	{
		m_Triangles[triIndex].p0 = vertices[indices[i]].position;
		m_Triangles[triIndex].p1 = vertices[indices[i + 1]].position;
		m_Triangles[triIndex].p2 = vertices[indices[i + 2]].position;
	}

	// Fill all triangle indices with their default value
	for (size_t i = 0; i < m_TriangleIndices.size(); ++i)
	{
		m_TriangleIndices[i] = i;
	}

	// Calculate all triangle centroids
	for (size_t i = 0; i < m_Triangles.size(); ++i)
	{
		m_TriangleCentroids[i] = GetTriangleCentroid(m_Triangles[i]);
	}

	// Set the first node to be the root node
	BVHNode& rootNode = m_BVHNodes[m_CurrentNodeIndex++];
	rootNode.leftFirst = 0;
	rootNode.numPrimitives = static_cast<uint32_t>(m_Triangles.size());

	CalculateNodeMinMax(rootNode);
	SubdivideNode(rootNode, 0);
}

uint32_t BVH::TraceRay(Ray& ray) const
{
	uint32_t primitiveID = ~0u;

	const BVHNode* bvhNode = &m_BVHNodes[0];
	const BVHNode* stack[64] = {};
	uint32_t stackPtr = 0;

	while (true)
	{
		// The current node is a leaf node, so we need to check intersections with the primitives
		if (bvhNode->numPrimitives > 0)
		{
			for (uint32_t triIndex = bvhNode->leftFirst; triIndex < bvhNode->leftFirst + bvhNode->numPrimitives; ++triIndex)
			{
				const Triangle triangle = m_Triangles[m_TriangleIndices[triIndex]];
				bool intersected = RTUtil::Intersect(triangle, ray);

				if (intersected)
				{
					primitiveID = triIndex;
				}
			}

			if (stackPtr == 0)
				break;
			else
				bvhNode = stack[--stackPtr];
			continue;
		}

		// If the current node is not a leaf node, keep traversing the left and right child nodes
		const BVHNode* leftChildNode = &m_BVHNodes[bvhNode->leftFirst];
		const BVHNode* rightChildNode = &m_BVHNodes[bvhNode->leftFirst + 1];

		// Determine distance to left and right child nodes, using SSE AABB intersections
		float leftDistance = RTUtil::IntersectSSE(leftChildNode->aabbMin4, leftChildNode->aabbMax4, ray);
		float rightDistance = RTUtil::IntersectSSE(rightChildNode->aabbMin4, rightChildNode->aabbMax4, ray);

		// Swap left and right child nodes so the closest is now the left child
		if (leftDistance > rightDistance)
		{
			std::swap(leftDistance, rightDistance);
			std::swap(leftChildNode, rightChildNode);
		}

		// If we have not intersected with the left and right child nodes, we can keep traversing the stack
		if (leftDistance == FLT_MAX)
		{
			if (stackPtr == 0)
				break;
			else
				bvhNode = stack[--stackPtr];
		}
		// We have intersected with either the left or right child node, so check the closest node first
		// and push the other one on the stack, if we have hit that one as well
		else
		{
			bvhNode = leftChildNode;
			if (rightDistance != FLT_MAX)
				stack[stackPtr++] = rightChildNode;
		}
	}

	return primitiveID;
}

Triangle BVH::GetTriangle(uint32_t primID) const
{
	return m_Triangles[m_TriangleIndices[primID]];
}

void BVH::CalculateNodeMinMax(BVHNode& bvhNode)
{
	bvhNode.aabbMin = glm::vec3(FLT_MAX);
	bvhNode.aabbMax = glm::vec3(-FLT_MAX);

	for (uint32_t i = bvhNode.leftFirst; i < bvhNode.leftFirst + bvhNode.numPrimitives; ++i)
	{
		const Triangle& triangle = m_Triangles[m_TriangleIndices[i]];

		glm::vec3 triangleMin, triangleMax;
		GetTriangleMinMax(triangle, triangleMin, triangleMax);

		bvhNode.aabbMin = glm::min(bvhNode.aabbMin, triangleMin);
		bvhNode.aabbMax = glm::max(bvhNode.aabbMax, triangleMax);
	}
}

void BVH::SubdivideNode(BVHNode& bvhNode, uint32_t depth)
{
	if (m_BuildHeuristic == BuildHeuristic_NaiveSplit)
	{
		// We can stop splitting, already 2 or less primitives in this node, making this a leaf node
		if (bvhNode.numPrimitives <= 2)
			return;

		const glm::vec3 extent = bvhNode.aabbMax - bvhNode.aabbMin;
		uint32_t axis = 0;

		// Determine which axis is the longest for the current node
		if (extent.y > extent.x)
			axis = 1;
		if (extent.z > extent[axis])
			axis = 2;

		// Split the node along the longest axis
		float splitPosition = bvhNode.aabbMin[axis] + extent[axis] * 0.5f;
		SplitNodeAlongAxisAtPosition(bvhNode, axis, splitPosition, depth);
	}
	else if (m_BuildHeuristic == BuildHeuristic_SAH_Intervals)
	{
		float parentNodeCost = bvhNode.numPrimitives * GetAABBVolume(bvhNode.aabbMin, bvhNode.aabbMax);

		float cheapestCost = FLT_MAX;
		uint32_t cheapestSplitAxis = 0;
		float cheapestSplitPosition = 0.0f;

		// Loop through each split interval and each axis, determine the cost for each split, and pick the cheapest one
		for (uint32_t splitInterval = 0; splitInterval < 8; ++splitInterval)
		{
			for (uint32_t currAxis = 0; currAxis < 3; ++currAxis)
			{
				float axisWidth = bvhNode.aabbMax[currAxis] - bvhNode.aabbMin[currAxis];
				float splitPosition = axisWidth * (static_cast<float>(splitInterval) / 8.0f) + bvhNode.aabbMin[currAxis];
				float splitCost = EvaluateSAHCost(bvhNode, currAxis, splitPosition);

				if (splitCost < cheapestCost)
				{
					cheapestCost = splitCost;
					cheapestSplitAxis = currAxis;
					cheapestSplitPosition = splitPosition;
				}
			}
		}

		// If there was no cheaper split than the parent split, its not worth to keep splitting, terminate
		if (cheapestCost >= parentNodeCost)
			return;

		SplitNodeAlongAxisAtPosition(bvhNode, cheapestSplitAxis, cheapestSplitPosition, depth);
	}
	else if (m_BuildHeuristic == BuildHeuristic_SAH_Primitives)
	{
		float parentNodeCost = bvhNode.numPrimitives * GetAABBVolume(bvhNode.aabbMin, bvhNode.aabbMax);

		float cheapestCost = FLT_MAX;
		uint32_t cheapestSplitAxis = 0;
		float cheapestSplitPosition = 0.0f;

		// Loop through each primitive and each axis, determine the cost for each split, and pick the cheapest one
		for (uint32_t triIndex = bvhNode.leftFirst; triIndex < bvhNode.leftFirst + bvhNode.numPrimitives; ++triIndex)
		{
			const glm::vec3& triangleCentroid = m_TriangleCentroids[m_TriangleIndices[triIndex]];

			for (uint32_t currAxis = 0; currAxis < 3; ++currAxis)
			{
				float axisWidth = bvhNode.aabbMax[currAxis] - bvhNode.aabbMin[currAxis];
				float splitPosition = triangleCentroid[currAxis];
				float splitCost = EvaluateSAHCost(bvhNode, currAxis, splitPosition);

				if (splitCost < cheapestCost)
				{
					cheapestCost = splitCost;
					cheapestSplitAxis = currAxis;
					cheapestSplitPosition = splitPosition;
				}
			}
		}

		// If there was no cheaper split than the parent split, its not worth to keep splitting, terminate
		if (cheapestCost >= parentNodeCost)
			return;

		SplitNodeAlongAxisAtPosition(bvhNode, cheapestSplitAxis, cheapestSplitPosition, depth);
	}
	else
	{
		FATAL_ERROR("BVH::SubdivideNode", "Invalid build heuristic");
	}
}

void BVH::SplitNodeAlongAxisAtPosition(BVHNode& bvhNode, uint32_t axis, float splitPosition, uint32_t depth)
{
	// Indices to the first and last triangle indices in this node
	int32_t i = bvhNode.leftFirst;
	int32_t j = i + bvhNode.numPrimitives - 1;

	// This will sort the triangles along the axis and split position
	while (i <= j)
	{
		// If the center position of the current triangle is on the left of the split position, continue to the next triangle
		if (m_TriangleCentroids[m_TriangleIndices[i]][axis] < splitPosition)
		{
			i++;
		}
		// If the center position of the current triangle is on the right, swap the current triangle index with one at the end, and then decrease the end index
		else
		{
			std::swap(m_TriangleIndices[i], m_TriangleIndices[j--]);
		}
	}

	// Determine how many nodes are on the left side of the split axis and position
	uint32_t numPrimitivesLeft = i - bvhNode.leftFirst;
	// If there is no or all primitives on the left side, we can stop splitting entirely
	if (numPrimitivesLeft == 0 || numPrimitivesLeft == bvhNode.numPrimitives)
		return;

	// Create two child nodes (left & right), and set their triangle start indices and count
	uint32_t leftChildNodeIndex = m_CurrentNodeIndex++;
	BVHNode& leftChildNode = m_BVHNodes[leftChildNodeIndex];
	leftChildNode.leftFirst = bvhNode.leftFirst;
	leftChildNode.numPrimitives = numPrimitivesLeft;

	uint32_t rightChildNodeIndex = m_CurrentNodeIndex++;
	BVHNode& rightChildNode = m_BVHNodes[rightChildNodeIndex];
	rightChildNode.leftFirst = i;
	rightChildNode.numPrimitives = bvhNode.numPrimitives - numPrimitivesLeft;

	// The current node we just split now becomes a parent node,
	// meaning it contains no primitives but points to the left and right nodes
	bvhNode.leftFirst = leftChildNodeIndex;
	bvhNode.numPrimitives = 0;

	// Calculate the node min max for the left and right child nodes
	CalculateNodeMinMax(leftChildNode);
	CalculateNodeMinMax(rightChildNode);

	// Subdivide the left and right child nodes
	SubdivideNode(leftChildNode, depth + 1);
	SubdivideNode(rightChildNode, depth + 1);
}

float BVH::EvaluateSAHCost(BVHNode& bvhNode, uint32_t axis, float splitPosition)
{
	// Set up the left and right side bounding min/max
	glm::vec3 leftAABBMin(FLT_MAX), leftAABBMax(-FLT_MAX);
	glm::vec3 rightAABBMin(FLT_MAX), rightAABBMax(-FLT_MAX);

	uint32_t numPrimitivesLeft = 0, numPrimitivesRight = 0;

	// Go through each triangle of the current node and split it along the axis and split position
	for (uint32_t triIndex = bvhNode.leftFirst; triIndex < bvhNode.leftFirst + bvhNode.numPrimitives; ++triIndex)
	{
		const Triangle& triangle = m_Triangles[m_TriangleIndices[triIndex]];
		const glm::vec3& triangleCentroid = m_TriangleCentroids[m_TriangleIndices[triIndex]];

		// Split by the triangle's centroid, and grow the corresponding AABB for the side the triangle is in
		if (triangleCentroid[axis] < splitPosition)
		{
			GrowAABB(leftAABBMin, leftAABBMax, triangle.p0);
			GrowAABB(leftAABBMin, leftAABBMax, triangle.p1);
			GrowAABB(leftAABBMin, leftAABBMax, triangle.p2);

			numPrimitivesLeft++;
		}
		else
		{
			GrowAABB(rightAABBMin, rightAABBMax, triangle.p0);
			GrowAABB(rightAABBMin, rightAABBMax, triangle.p1);
			GrowAABB(rightAABBMin, rightAABBMax, triangle.p2);

			numPrimitivesRight++;
		}
	}

	// Determine the total cost of the split
	float splitCost = numPrimitivesLeft * GetAABBVolume(leftAABBMin, leftAABBMax) +
		numPrimitivesRight * GetAABBVolume(rightAABBMin, rightAABBMax);

	return splitCost;
}

glm::vec3 BVH::GetTriangleCentroid(const Triangle& triangle) const
{
	return (triangle.p0 + triangle.p1 + triangle.p2) * 0.3333f;
}

void BVH::GetTriangleMinMax(const Triangle& triangle, glm::vec3& outMin, glm::vec3& outMax) const
{
	outMin = glm::min(triangle.p0, triangle.p1);
	outMax = glm::max(triangle.p0, triangle.p1);

	outMin = glm::min(outMin, triangle.p2);
	outMax = glm::max(outMax, triangle.p2);
}

float BVH::GetAABBVolume(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const
{
	const glm::vec3 extent = aabbMax - aabbMin;
	return extent.x * extent.y + extent.y * extent.z + extent.z * extent.x;
}

void BVH::GrowAABB(glm::vec3& aabbMin, glm::vec3& aabbMax, const glm::vec3& pos) const
{
	aabbMin = glm::min(aabbMin, pos);
	aabbMax = glm::max(aabbMax, pos);
}
