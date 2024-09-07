#pragma once
#include "renderer/raytracing_types.h"

class bvh_t;

class bvh_instance_t
{
public:
	void set_bvh(const bvh_t* Bvh);
	glm::mat4 get_world_to_local_transform() const;
	void set_transform(const glm::mat4& transform);
	const triangle_t* trace_ray(ray_t& world_ray, hit_result_t& hit_result_t) const;

	aabb_t get_world_space_aabb() const;

private:
	const bvh_t* m_blas = nullptr;

	glm::mat4 m_local_to_world_transform = glm::identity<glm::mat4>();
	glm::mat4 m_world_to_local_transform = glm::identity<glm::mat4>();

	aabb_t m_world_space_aabb = {};

};
