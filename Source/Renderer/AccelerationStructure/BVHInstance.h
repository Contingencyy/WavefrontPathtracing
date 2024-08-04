#pragma once
#include "Renderer/RaytracingTypes.h"

class BVH;

class BVHInstance
{
public:
	void SetBVH(const BVH* Bvh);
	glm::mat4 GetWorldToLocalTransform() const;
	void SetTransform(const glm::mat4& Transform);
	const Triangle* TraceRay(Ray& WorldRay, HitResult& HitResult) const;

	AABB GetWorldSpaceAABB() const;

private:
	const BVH* m_BLAS = nullptr;

	glm::mat4 m_LocalToWorldTransform = glm::identity<glm::mat4>();
	glm::mat4 m_WorldToLocalTransform = glm::identity<glm::mat4>();

	AABB m_WorldSpaceAABB = {};

};
