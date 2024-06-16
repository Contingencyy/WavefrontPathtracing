#pragma once
#include "Renderer/RaytracingTypes.h"

class BVH;

class BVHInstance
{
public:
	BVHInstance() = default;
	BVHInstance(const BVH* blas);

	void SetTransform(const glm::mat4& transform);
	uint32_t TraceRay(Ray& ray) const;

	AABB GetWorldSpaceAABB() const;
	glm::vec3 GetNormal(uint32_t primitiveIndex) const;

private:
	const BVH* m_BLAS = nullptr;

	glm::mat4 m_LocalToWorldTransform = glm::identity<glm::mat4>();
	glm::mat4 m_WorldToLocalTransform = glm::identity<glm::mat4>();

	AABB m_WorldSpaceAABB = {};

};
