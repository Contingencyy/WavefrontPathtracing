#include "pch.h"
#include "bvh_instance.h"
#include "bvh.h"
#include "renderer/raytracing_utils.h"

void bvh_instance_t::set_bvh(const bvh_t* bvh)
{
	m_blas = bvh;
}

glm::mat4 bvh_instance_t::get_world_to_local_transform() const
{
	return m_world_to_local_transform;
}

void bvh_instance_t::set_transform(const glm::mat4& transform)
{
	// Set the world to local (bvh) transform, which will be used to transform the rays into the local space when tracing
	m_local_to_world_transform = transform;
	m_world_to_local_transform = glm::inverse(transform);

	// Calculate the world space bounding box of the root node
	aabb_t bvh_local_aabb = m_blas->get_local_space_aabb();
	m_world_space_aabb.pmin = glm::vec3(FLT_MAX);
	m_world_space_aabb.pmax = glm::vec3(-FLT_MAX);

	// Grow the world-space bounding box by the eight corners of the root node aabb_t, which is in local space
	for (u32 i = 0; i < 8; ++i)
	{
		glm::vec3 worldPosition = m_local_to_world_transform *
			glm::vec4(i & 1 ? bvh_local_aabb.pmax.x : bvh_local_aabb.pmin.x, i & 2 ? bvh_local_aabb.pmax.y : bvh_local_aabb.pmin.y, i & 4 ? bvh_local_aabb.pmax.z : bvh_local_aabb.pmin.z, 1.0f);
		rt_util::grow_aabb(m_world_space_aabb.pmin, m_world_space_aabb.pmax, worldPosition);
	}
}

const triangle_t* bvh_instance_t::trace_ray(ray_t& ray_world, hit_result_t& hit_result_t) const
{
	// Create a new ray that is in object-space of the bvh_t we want to intersect
	// This is the same as if we transformed the bvh_t to world-space instead
	ray_t ray_intersect = ray_world;
	ray_intersect.origin = m_world_to_local_transform * glm::vec4(ray_world.origin, 1.0f);
	ray_intersect.dir = m_world_to_local_transform * glm::vec4(ray_world.dir, 0.0f);
	ray_intersect.inv_dir = 1.0f / ray_intersect.dir;

	// Trace ray through the bvh_t
	const triangle_t* hit_triangle = m_blas->trace_ray(ray_intersect, hit_result_t);

	// update the ray with the object-space ray depth
	ray_world.t = ray_intersect.t;
	ray_world.bvh_depth = ray_intersect.bvh_depth;

	if (hit_triangle)
	{
		hit_result_t.pos = ray_world.origin + ray_world.dir * ray_world.t;
		glm::vec4 normal_world = m_local_to_world_transform * glm::vec4(rt_util::get_hit_normal(*hit_triangle, hit_result_t.bary), 0.0f);
		hit_result_t.normal = glm::normalize(normal_world);
	}

	return hit_triangle;
}

aabb_t bvh_instance_t::get_world_space_aabb() const
{
	return m_world_space_aabb;
}
