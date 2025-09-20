#pragma once
#include "core/common.h"

namespace as_util
{

	inline float get_aabb_volume(const glm::vec3& aabb_min, const glm::vec3& aabb_max)
	{
		const glm::vec3 extent = aabb_max - aabb_min;
		return extent.x * extent.y + extent.y * extent.z + extent.z * extent.x;
	}

	inline void grow_aabb(glm::vec3& aabb_min, glm::vec3& aabb_max, const glm::vec3& pos)
	{
		aabb_min = glm::min(aabb_min, pos);
		aabb_max = glm::max(aabb_max, pos);
	}

	inline void grow_aabb(glm::vec3& aabb_min, glm::vec3& aabb_max, const glm::vec3& other_min, const glm::vec3& other_max)
	{
		aabb_min = glm::min(aabb_min, other_min);
		aabb_max = glm::max(aabb_max, other_max);
	}

}
