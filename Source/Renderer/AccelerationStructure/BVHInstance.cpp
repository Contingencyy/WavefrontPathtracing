#include "Pch.h"
#include "BVHInstance.h"
#include "BVH.h"
#include "Renderer/RaytracingUtils.h"

void BVHInstance::SetBVH(const BVH* Bvh)
{
	m_BLAS = Bvh;
}

glm::mat4 BVHInstance::GetWorldToLocalTransform() const
{
	return m_WorldToLocalTransform;
}

void BVHInstance::SetTransform(const glm::mat4& Transform)
{
	// Set the world to local (bvh) transform, which will be used to transform the rays into the local space when tracing
	m_LocalToWorldTransform = Transform;
	m_WorldToLocalTransform = glm::inverse(Transform);

	// Calculate the world space bounding box of the root node
	AABB bvhLocalAABB = m_BLAS->GetLocalSpaceAABB();
	m_WorldSpaceAABB.PMin = glm::vec3(FLT_MAX);
	m_WorldSpaceAABB.PMax = glm::vec3(-FLT_MAX);

	// Grow the world-space bounding box by the eight corners of the root node AABB, which is in local space
	for (u32 i = 0; i < 8; ++i)
	{
		glm::vec3 worldPosition = m_LocalToWorldTransform *
			glm::vec4(i & 1 ? bvhLocalAABB.PMax.x : bvhLocalAABB.PMin.x, i & 2 ? bvhLocalAABB.PMax.y : bvhLocalAABB.PMin.y, i & 4 ? bvhLocalAABB.PMax.z : bvhLocalAABB.PMin.z, 1.0f);
		RTUtil::GrowAABB(m_WorldSpaceAABB.PMin, m_WorldSpaceAABB.PMax, worldPosition);
	}
}

const Triangle* BVHInstance::TraceRay(Ray& WorldRay, HitResult& HitResult) const
{
	// Create a new ray that is in object-space of the BVH we want to intersect
	// This is the same as if we transformed the BVH to world-space instead
	Ray IntersectRay = WorldRay;
	IntersectRay.Origin = m_WorldToLocalTransform * glm::vec4(WorldRay.Origin, 1.0f);
	IntersectRay.Dir = m_WorldToLocalTransform * glm::vec4(WorldRay.Dir, 0.0f);
	IntersectRay.InvDir = 1.0f / IntersectRay.Dir;

	// Trace ray through the BVH
	const Triangle* HitTri = m_BLAS->TraceRay(IntersectRay, HitResult);

	// Update the ray with the object-space ray depth
	WorldRay.t = IntersectRay.t;
	WorldRay.BvhDepth = IntersectRay.BvhDepth;

	if (HitTri)
	{
		HitResult.Pos = WorldRay.Origin + WorldRay.Dir * WorldRay.t;
		glm::vec4 worldSpaceNormal = m_LocalToWorldTransform * glm::vec4(RTUtil::GetHitNormal(*HitTri, HitResult.Bary), 0.0f);
		HitResult.Normal = glm::normalize(worldSpaceNormal);
	}

	return HitTri;
}

AABB BVHInstance::GetWorldSpaceAABB() const
{
	return m_WorldSpaceAABB;
}
