#include "Pch.h"
#include "BVHInstance.h"
#include "BVH.h"
#include "Renderer/RaytracingUtils.h"

BVHInstance::BVHInstance(const BVH* blas)
	: m_BLAS(blas)
{
}

void BVHInstance::SetTransform(const glm::mat4& transform)
{
	// Set the world to local (bvh) transform, which will be used to transform the rays into the local space when tracing
	m_LocalToWorldTransform = transform;
	m_WorldToLocalTransform = glm::inverse(transform);

	// Calculate the world space bounding box of the root node
	AABB bvhLocalAABB = m_BLAS->GetLocalSpaceAABB();
	m_WorldSpaceAABB.pmin = glm::vec3(FLT_MAX);
	m_WorldSpaceAABB.pmax = glm::vec3(-FLT_MAX);

	// Grow the world-space bounding box by the eight corners of the root node AABB, which is in local space
	for (uint32_t i = 0; i < 8; ++i)
	{
		glm::vec3 worldPosition = transform *
			glm::vec4(i & 1 ? bvhLocalAABB.pmax.x : bvhLocalAABB.pmin.x, i & 2 ? bvhLocalAABB.pmax.y : bvhLocalAABB.pmin.y, i & 4 ? bvhLocalAABB.pmax.z : bvhLocalAABB.pmin.z, 1.0f);
		RTUtil::GrowAABB(m_WorldSpaceAABB.pmin, m_WorldSpaceAABB.pmax, worldPosition);
	}
}

uint32_t BVHInstance::TraceRay(Ray& ray) const
{
	// Make a copy of the original ray since we need to revert the transform once we are done tracing this BVH
	Ray originalRay = ray;
	ray.origin = m_WorldToLocalTransform * glm::vec4(ray.origin, 1.0f);
	ray.dir = m_WorldToLocalTransform * glm::vec4(ray.dir, 0.0f);
	ray.invDir = 1.0f / ray.dir;

	// Trace ray through the BVH
	uint32_t primitiveIndex = m_BLAS->TraceRay(ray);

	// Restore original ray origin and direction before transform
	originalRay.t = ray.t;
	originalRay.bvhDepth = ray.bvhDepth;
	ray = originalRay;

	return primitiveIndex;
}

AABB BVHInstance::GetWorldSpaceAABB() const
{
	return m_WorldSpaceAABB;
}

glm::vec3 BVHInstance::GetNormal(uint32_t primitiveIndex) const
{
	return m_LocalToWorldTransform * glm::vec4(RTUtil::GetHitNormal(m_BLAS->GetTriangle(primitiveIndex)), 0.0f);
}
