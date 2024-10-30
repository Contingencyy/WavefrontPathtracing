#pragma once
#include "renderer/raytracing_types.h"
#include "renderer/renderer_fwd.h"

struct memory_arena_t;
struct vertex_t;

struct bvh_triangle_t
{
	glm::vec3 p0 = {};
	glm::vec3 p1 = {};
	glm::vec3 p2 = {};
};

struct bvh_node_t
{
	union { struct { glm::vec3 aabb_min; u32 left_first; }; __m128 aabb_min4 = {}; };
	union { struct { glm::vec3 aabb_max; u32 prim_count; }; __m128 aabb_max4 = {}; };
};

struct bvh_t
{
	bvh_node_t* nodes = nullptr;
	bvh_triangle_t* triangles = nullptr;
	u32* triangle_indices = nullptr;
};

struct bvh_instance_t
{
	glm::mat4 local_to_world_transform = glm::identity<glm::mat4>();
	glm::mat4 world_to_local_transform = glm::identity<glm::mat4>();
	aabb_t aabb_world;
	render_mesh_handle_t mesh_handle = {};
};

class bvh_builder_t
{
public:
	struct build_options_t
	{
		u32 interval_count = 8;
		b8 subdivide_single_prim = false;
	};

	struct build_args_t
	{
		vertex_t* vertices = nullptr;
		u32 vertex_count = 0;
		u32* indices = nullptr;
		u32 index_count = 0;

		build_options_t options;
	};

public:
	void build(memory_arena_t* arena, const build_args_t& build_args);
	bvh_t extract(memory_arena_t* arena) const;

private:
	void calc_node_min_max(bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max);
	f32 calc_node_cost(const bvh_node_t& node) const;

	void subdivide_node(memory_arena_t* arena, bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max, u32 depth);
	f32 find_best_split_plane(memory_arena_t* arena, bvh_node_t& node, const glm::vec3& centroid_min, const glm::vec3& centroid_max, u32& out_axis, u32& out_split_pos);

	glm::vec3 get_triangle_centroid(const bvh_triangle_t& triangle);
	void get_triangle_min_max(const bvh_triangle_t& triangle, glm::vec3& out_min, glm::vec3& out_max);

private:
	struct bvh_bin_t
	{
		glm::vec3 aabb_min;
		glm::vec3 aabb_max;
		u32 prim_count;
	};

private:
	build_options_t m_build_opts = {};

	u32 m_node_count;
	u32 m_node_at;
	bvh_node_t* m_nodes;

	u32 m_triangle_count;
	bvh_triangle_t* m_triangles;
	u32* m_triangle_indices;
	glm::vec3* m_triangle_centroids;

};
