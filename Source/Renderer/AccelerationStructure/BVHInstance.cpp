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

const Triangle* BVHInstance::TraceRay(Ray& ray, HitResult& hitResult) const
{
	// Create a new ray that is in object-space of the BVH we want to intersect
	// This is the same as if we transformed the BVH to world-space instead
	Ray intersectRay = ray;
	intersectRay.origin = m_WorldToLocalTransform * glm::vec4(ray.origin, 1.0f);
	intersectRay.dir = m_WorldToLocalTransform * glm::vec4(ray.dir, 0.0f);
	intersectRay.invDir = 1.0f / intersectRay.dir;

	// Trace ray through the BVH
	const Triangle* hitTri = m_BLAS->TraceRay(intersectRay, hitResult);

	// Update the ray with the object-space ray depth
	ray.t = intersectRay.t;
	ray.bvhDepth = intersectRay.bvhDepth;

	if (hitTri)
	{
		hitResult.pos = ray.origin + ray.dir * ray.t;
		glm::vec4 worldSpaceNormal = m_LocalToWorldTransform * glm::vec4(RTUtil::GetHitNormal(*hitTri, hitResult.bary), 0.0f);
		hitResult.normal = glm::normalize(worldSpaceNormal);
	}

	return hitTri;
}

AABB BVHInstance::GetWorldSpaceAABB() const
{
	return m_WorldSpaceAABB;
}
