#pragma once
#include "renderer/shaders/shared.hlsl.h"

struct memory_arena_t;
struct triangle_t;

struct bvh_t
{
	bvh_header_t header;
	void* data;
};

class bvh_builder_t
{
public:
	struct build_options_t
	{
		uint32_t interval_count;
		bool subdivide_single_prim;
	};

	struct build_args_t
	{
		const triangle_t* triangles;
		uint32_t triangle_count;

		build_options_t options;
	};

public:
	void build(memory_arena_t* arena, const build_args_t& build_args);
	void extract(memory_arena_t* arena, bvh_t& out_bvh, uint64_t& out_bvh_byte_size) const;

private:
	void calc_node_min_max(bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max);
	float calc_node_cost(const bvh_node_t& node) const;

	void subdivide_node(memory_arena_t* arena, bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max, uint32_t depth);
	float find_best_split_plane(memory_arena_t* arena, bvh_node_t& node, const glm::vec3& centroid_min, const glm::vec3& centroid_max, uint32_t& out_axis, uint32_t& out_split_pos);

	glm::vec3 get_triangle_centroid(const bvh_triangle_t& triangle);
	void get_triangle_min_max(const bvh_triangle_t& triangle, glm::vec3& out_min, glm::vec3& out_max);

private:
	struct bvh_bin_t
	{
		glm::vec3 aabb_min;
		glm::vec3 aabb_max;
		uint32_t prim_count;
	};

private:
	build_options_t m_build_opts = {};

	uint32_t m_node_count;
	uint32_t m_node_at;
	bvh_node_t* m_nodes;

	uint32_t m_triangle_count;
	bvh_triangle_t* m_triangles;
	uint32_t* m_triangle_indices;
	glm::vec3* m_triangle_centroids;

};
