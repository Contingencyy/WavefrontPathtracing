#pragma once
#include "renderer/raytracing_types.h"

struct ray_t;
class bvh_instance_t;

class tlas_t
{
public:
	void build(memory_arena_t* arena, bvh_instance_t* blas, u32 blas_count);
	void trace_ray(ray_t& ray_t, hit_result_t& hit_result_t) const;

private:
	struct tlas_node_t
	{
		// left_right will store two separate indices, 16 bit for each
		union { struct { glm::vec3 aabb_min; u32 left_right; }; __m128 aabb_min4 = {}; };
		union { struct { glm::vec3 aabb_max; u32 blas_instance_index; }; __m128 aabb_max4 = {}; };

		b8 is_leaf() const;
	};

private:
	u32 find_best_match(u32 A, const u32* indices, u32 index_count);

private:
	u32 m_node_count;
	u32 m_node_at = 0;
	tlas_node_t* m_nodes;

	u32 m_blas_instance_count;
	bvh_instance_t* m_blas_instances;

};
