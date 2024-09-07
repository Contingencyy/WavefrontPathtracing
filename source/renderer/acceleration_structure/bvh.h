#pragma once
#include "renderer/raytracing_types.h"

struct vertex_t;

class bvh_t
{
public:
	struct build_options_t
	{
		u32 interval_count = 8;
		b8 subdivide_single_prim = false;
	};

public:
	void build(memory_arena_t* arena, vertex_t* vertices, u32 vertex_count, u32* indices, u32 index_count, const build_options_t& build_opts);

	const triangle_t* trace_ray(ray_t& ray, hit_result_t& hit_result) const;
	aabb_t get_local_space_aabb() const;

private:
	struct bvh_node_t
	{
		union { struct { glm::vec3 aabb_min; u32 left_first; }; __m128 aabb_min4 = {}; };
		union { struct { glm::vec3 aabb_max; u32 prim_count; }; __m128 aabb_max4 = {}; };
	};

	struct bvh_bin_t
	{
		glm::vec3 aabb_min;
		glm::vec3 aabb_max;
		u32 prim_count;
	};

private:
	void calc_node_min_max(bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max);
	f32 calc_node_cost(const bvh_node_t& node) const;

	void subdivide_node(memory_arena_t* arena, bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max, u32 depth);
	f32 find_best_split_plane(memory_arena_t* arena, bvh_node_t& node, const glm::vec3& centroid_min, const glm::vec3& centroid_max, u32& out_axis, u32& out_split_pos);

private:
	build_options_t m_build_opts = {};

	u32 m_node_count;
	u32 m_node_at;
	bvh_node_t* m_nodes;
	
	u32 m_tri_count;
	triangle_t* m_tris;
	u32* m_tri_indices;
	glm::vec3* m_tri_centroids;

};
