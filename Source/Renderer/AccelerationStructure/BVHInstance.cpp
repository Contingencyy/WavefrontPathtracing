#include "Pch.h"
#include "BVHInstance.h"
#include "BVH.h"
#include "Renderer/RaytracingUtils.h"

BVHInstance::BVHInstance(const BVH* blas)
	: m_BLAS(blas)
{
}

glm::mat4 BVHInstance::GetWorldToLocalTransform() const
{
	return m_WorldToLocalTransform;
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
		glm::vec3 worldPosition = m_LocalToWorldTransform *
			glm::vec4(i & 1 ? bvhLocalAABB.pmax.x : bvhLocalAABB.pmin.x, i & 2 ? bvhLocalAABB.pmax.y : bvhLocalAABB.pmin.y, i & 4 ? bvhLocalAABB.pmax.z : bvhLocalAABB.pmin.z, 1.0f);
		RTUtil::GrowAABB(m_WorldSpaceAABB.pmin, m_WorldSpaceAABB.pmax, worldPosition);
	}
}

uint32_t BVHInstance::TraceRay(Ray& ray) const
{
	// Trace ray through the BVH
	return m_BLAS->TraceRay(ray);
}

AABB BVHInstance::GetWorldSpaceAABB() const
{
	return m_WorldSpaceAABB;
}

glm::vec3 BVHInstance::GetNormal(uint32_t primitiveIndex) const
{
	return glm::normalize(m_LocalToWorldTransform * glm::vec4(RTUtil::GetHitNormal(m_BLAS->GetTriangle(primitiveIndex)), 0.0f));
}
