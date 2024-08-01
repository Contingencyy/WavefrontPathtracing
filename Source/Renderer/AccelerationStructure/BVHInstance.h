#pragma once
#include "Renderer/RaytracingTypes.h"

class BVH;

class BVHInstance
{
public:
	BVHInstance() = default;
	BVHInstance(const BVH* blas);

	glm::mat4 GetWorldToLocalTransform() const;
	void SetTransform(const glm::mat4& transform);
	const Triangle* TraceRay(Ray& ray, HitResult& hitResult) const;

	AABB GetWorldSpaceAABB() const;

private:
	const BVH* m_BLAS = nullptr;

	glm::mat4 m_LocalToWorldTransform = glm::identity<glm::mat4>();
	glm::mat4 m_WorldToLocalTransform = glm::identity<glm::mat4>();

	AABB m_WorldSpaceAABB = {};

};
