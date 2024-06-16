#include "Pch.h"
#include "BVH.h"
#include "Renderer/RaytracingUtils.h"

void BVH::Build(const std::vector<Renderer::Vertex>& vertices, const std::vector<uint32_t>& indices, const BuildOptions& buildOptions)
{
	m_BuildOptions = buildOptions;

	m_Triangles.resize(indices.size() / 3);
	m_TriangleIndices.resize(m_Triangles.size());
	m_TriangleCentroids.resize(m_Triangles.size());
	m_BVHNodes.resize(m_Triangles.size() * 2);

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
		m_TriangleCentroids[i] = RTUtil::GetTriangleCentroid(m_Triangles[i]);
	}

	// Set the first node to be the root node
	BVHNode& rootNode = m_BVHNodes[m_CurrentNodeIndex];
	rootNode.leftFirst = 0;
	rootNode.numPrimitives = static_cast<uint32_t>(m_Triangles.size());

	// Skip over m_BVHNodes[1] for cache alignment
	m_CurrentNodeIndex = 2;

	glm::vec3 centroidMin, centroidMax;
	CalculateNodeMinMax(rootNode, centroidMin, centroidMax);
	SubdivideNode(rootNode, centroidMin, centroidMax, 0);
}

uint32_t BVH::TraceRay(Ray& ray) const
{
	// Make a copy of the original ray since we need to revert the transform once we are done tracing this BVH
	Ray originalRay = ray;
	ray.origin = m_WorldToLocalTransform * glm::vec4(ray.origin, 1.0f);
	ray.dir = m_WorldToLocalTransform * glm::vec4(ray.dir, 0.0f);
	ray.invDir = 1.0f / ray.dir;

	uint32_t primitiveIndex = ~0u;

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
					primitiveIndex = triIndex;
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

		// If we have not intersected with the left and right child nodes, we can keep traversing the stack, or terminate if stack is empty
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
			ray.bvhDepth++;

			bvhNode = leftChildNode;
			if (rightDistance != FLT_MAX)
				stack[stackPtr++] = rightChildNode;
		}
	}

	// Update the original ray's depth, and set the ray back to its original pre-transform state
	originalRay.t = ray.t;
	originalRay.bvhDepth = ray.bvhDepth;
	ray = originalRay;

	return primitiveIndex;
}

Triangle BVH::GetTriangle(uint32_t primID) const
{
	return m_Triangles[m_TriangleIndices[primID]];
}

AABB BVH::GetWorldSpaceBoundingBox() const
{
	return m_WorldSpaceBoundingBox;
}

void BVH::SetTransform(const glm::mat4& transform)
{
	// Set the world to local (bvh) transform, which will be used to transform the rays into the local space when tracing
	m_WorldToLocalTransform = glm::inverse(transform);
	
	// Calculate the world space bounding box of the root node
	glm::vec3 worldMin = m_BVHNodes[0].aabbMin, worldMax = m_BVHNodes[0].aabbMax;
	m_WorldSpaceBoundingBox.pmin = glm::vec3(FLT_MAX);
	m_WorldSpaceBoundingBox.pmax = glm::vec3(-FLT_MAX);

	// Grow the world-space bounding box by the eight corners of the root node AABB, which is in local space
	for (uint32_t i = 0; i < 8; ++i)
	{
		glm::vec3 worldPosition = transform *
			glm::vec4(i & 1 ? worldMax.x : worldMin.x, i & 2 ? worldMax.y : worldMin.y, i & 4 ? worldMax.z : worldMin.z, 1.0f);
		RTUtil::GrowAABB(m_WorldSpaceBoundingBox.pmin, m_WorldSpaceBoundingBox.pmax, worldPosition);
	}
}

void BVH::CalculateNodeMinMax(BVHNode& bvhNode, glm::vec3& centroidMin, glm::vec3& centroidMax)
{
	bvhNode.aabbMin = glm::vec3(FLT_MAX);
	bvhNode.aabbMax = glm::vec3(-FLT_MAX);
	centroidMin = glm::vec3(FLT_MAX);
	centroidMax = glm::vec3(-FLT_MAX);

	for (uint32_t triIndex = bvhNode.leftFirst; triIndex < bvhNode.leftFirst + bvhNode.numPrimitives; ++triIndex)
	{
		const Triangle& triangle = m_Triangles[m_TriangleIndices[triIndex]];

		glm::vec3 triangleMin, triangleMax;
		RTUtil::GetTriangleMinMax(triangle, triangleMin, triangleMax);

		bvhNode.aabbMin = glm::min(bvhNode.aabbMin, triangleMin);
		bvhNode.aabbMax = glm::max(bvhNode.aabbMax, triangleMax);

		const glm::vec3& centroid = m_TriangleCentroids[m_TriangleIndices[triIndex]];

		centroidMin = glm::min(centroidMin, centroid);
		centroidMax = glm::max(centroidMax, centroid);
	}
}

float BVH::CalculateNodeCost(const BVHNode& bvhNode) const
{
	return bvhNode.numPrimitives * RTUtil::GetAABBVolume(bvhNode.aabbMin, bvhNode.aabbMax);
}

void BVH::SubdivideNode(BVHNode& bvhNode, glm::vec3& centroidMin, glm::vec3& centroidMax, uint32_t depth)
{
	uint32_t splitAxis = 0;
	uint32_t splitPosition = 0.0f;
	float splitCost = FindBestSplitPlane(bvhNode, centroidMin, centroidMax, splitAxis, splitPosition);

	// If subdivideToSinglePrimitive is enabled in the build options, we always reduce the BVH down to a single primitive per leaf-node
	if (m_BuildOptions.subdivideToSinglePrimitive)
	{
		if (bvhNode.numPrimitives == 1)
			return;
	}
	// Otherwise we compare the cost of doing the split to the parent node cost, and terminate if a split is not worth it
	else
	{
		float parentNodeCost = CalculateNodeCost(bvhNode);
		if (splitCost >= parentNodeCost)
			return;
	}

	// Indices to the first and last triangle indices in this node
	int32_t i = bvhNode.leftFirst;
	int32_t j = i + bvhNode.numPrimitives - 1;
	float binScale = m_BuildOptions.numIntervals / (centroidMax[splitAxis] - centroidMin[splitAxis]);

	// This will sort the triangles along the axis and split position
	while (i <= j)
	{
		const glm::vec3& centroid = m_TriangleCentroids[m_TriangleIndices[i]];

		int32_t binIndex = glm::min((int32_t)m_BuildOptions.numIntervals - 1,
			(int32_t)((centroid[splitAxis] - centroidMin[splitAxis]) * binScale));

		if (binIndex < splitPosition)
			i++;
		else
			std::swap(m_TriangleIndices[i], m_TriangleIndices[j--]);
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

	// Calculate min/max and subdivide left child node
	CalculateNodeMinMax(leftChildNode, centroidMin, centroidMax);
	SubdivideNode(leftChildNode, centroidMin, centroidMax, depth + 1);

	// Calculate min/max and subdivide right child node
	CalculateNodeMinMax(rightChildNode, centroidMin, centroidMax);
	SubdivideNode(rightChildNode, centroidMin, centroidMax, depth + 1);
}

float BVH::FindBestSplitPlane(BVHNode& bvhNode, const glm::vec3& centroidMin, const glm::vec3& centroidMax, uint32_t& outAxis, uint32_t& outSplitPosition)
{
	float cheapestCost = FLT_MAX;

	for (uint32_t axis = 0; axis < 3; ++axis)
	{
		float boundsMin = centroidMin[axis], boundsMax = centroidMax[axis];
		if (boundsMin == boundsMax)
			continue;

		std::vector<BVHBin> bvhBins(m_BuildOptions.numIntervals);
		float binScale = m_BuildOptions.numIntervals / (boundsMax - boundsMin);

		// Grow the bins
		for (uint32_t triIndex = bvhNode.leftFirst; triIndex < bvhNode.leftFirst + bvhNode.numPrimitives; ++triIndex)
		{
			const Triangle& triangle = m_Triangles[m_TriangleIndices[triIndex]];
			const glm::vec3& centroid = m_TriangleCentroids[m_TriangleIndices[triIndex]];

			int32_t binIndex = glm::min((int32_t)m_BuildOptions.numIntervals - 1,
				(int32_t)((centroid[axis] - boundsMin) * binScale));

			BVHBin& bvhBin = bvhBins[binIndex];
			bvhBin.numPrimitives++;
			RTUtil::GrowAABB(bvhBin.aabbMin, bvhBin.aabbMax, triangle.p0);
			RTUtil::GrowAABB(bvhBin.aabbMin, bvhBin.aabbMax, triangle.p1);
			RTUtil::GrowAABB(bvhBin.aabbMin, bvhBin.aabbMax, triangle.p2);
		}

		// Get all necessary data for the planes between the bins
		std::vector<float> leftArea(m_BuildOptions.numIntervals - 1), rightArea(m_BuildOptions.numIntervals - 1);
		
		glm::vec3 leftAABBMin(FLT_MAX), leftAABBMax(-FLT_MAX);
		glm::vec3 rightAABBMin(FLT_MAX), rightAABBMax(-FLT_MAX);

		uint32_t leftSum = 0, rightSum = 0;

		for (uint32_t binIndex = 0; binIndex < m_BuildOptions.numIntervals - 1; ++binIndex)
		{
			const BVHBin& leftBin = bvhBins[binIndex];

			leftSum += leftBin.numPrimitives;
			RTUtil::GrowAABB(leftAABBMin, leftAABBMax, leftBin.aabbMin, leftBin.aabbMax);
			leftArea[binIndex] = leftSum * RTUtil::GetAABBVolume(leftAABBMin, leftAABBMax);

			const BVHBin& rightBin = bvhBins[m_BuildOptions.numIntervals - 1 - binIndex];
			rightSum += rightBin.numPrimitives;
			RTUtil::GrowAABB(rightAABBMin, rightAABBMax, rightBin.aabbMin, rightBin.aabbMax);
			rightArea[m_BuildOptions.numIntervals - 2 - binIndex] = rightSum * RTUtil::GetAABBVolume(rightAABBMin, rightAABBMax);
		}

		// Evaluate SAH cost for each plane
		binScale = (boundsMax - boundsMin) / m_BuildOptions.numIntervals;

		for (uint32_t binIndex = 0; binIndex < m_BuildOptions.numIntervals - 1; ++binIndex)
		{
			float planeCost = leftArea[binIndex] + rightArea[binIndex];

			if (planeCost < cheapestCost)
			{
				outAxis = axis;
				outSplitPosition = binIndex + 1;
				cheapestCost = planeCost;
			}
		}
	}

	return cheapestCost;
}
