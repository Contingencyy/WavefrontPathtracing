#include "pch.h"
#include "bvh_builder.h"
#include "core/memory/memory_arena.h"

#include "renderer/renderer_fwd.h"
#include "renderer/raytracing_types.h"
#include "renderer/raytracing_utils.h"

void bvh_builder_t::build(memory_arena_t* arena, const build_args_t& build_args)
{
	m_build_opts = build_args.options;

	m_tri_count = build_args.index_count / 3;
	m_tris = ARENA_ALLOC_ARRAY_ZERO(arena, triangle_t, m_tri_count);
	m_tri_indices = ARENA_ALLOC_ARRAY_ZERO(arena, u32, m_tri_count);
	m_tri_centroids = ARENA_ALLOC_ARRAY_ZERO(arena, glm::vec3, m_tri_count);

	m_node_count = m_tri_count * 2;
	m_nodes = ARENA_ALLOC_ARRAY_ZERO(arena, bvh_node_t, m_node_count);
	m_node_at = 0;

	// Make bvh_t triangles from vertices and indices
	for (u32 tri_idx = 0, i = 0; tri_idx < m_tri_count; ++tri_idx, i += 3)
	{
		m_tris[tri_idx].p0 = build_args.vertices[build_args.indices[i]].position;
		m_tris[tri_idx].p1 = build_args.vertices[build_args.indices[i + 1]].position;
		m_tris[tri_idx].p2 = build_args.vertices[build_args.indices[i + 2]].position;

		m_tris[tri_idx].n0 = build_args.vertices[build_args.indices[i]].normal;
		m_tris[tri_idx].n1 = build_args.vertices[build_args.indices[i + 1]].normal;
		m_tris[tri_idx].n2 = build_args.vertices[build_args.indices[i + 2]].normal;
	}

	// Fill all triangle indices with their default value
	for (u32 i = 0; i < m_tri_count; ++i)
	{
		m_tri_indices[i] = i;
	}

	// Calculate all triangle centroids
	for (size_t i = 0; i < m_tri_count; ++i)
	{
		m_tri_centroids[i] = rt_util::get_triangle_centroid(m_tris[i]);
	}

	// Set the first node to be the root node
	bvh_node_t& root_node = m_nodes[m_node_at];
	root_node.left_first = 0;
	root_node.prim_count = m_tri_count;

	// Skip over m_BVHNodes[1] for cache alignment
	m_node_at = 2;

	glm::vec3 node_centroid_min, node_centroid_max;
	calc_node_min_max(root_node, node_centroid_min, node_centroid_max);
	subdivide_node(arena, root_node, node_centroid_min, node_centroid_max, 0);
}

bvh_t bvh_builder_t::extract(memory_arena_t* arena)
{
	bvh_t bvh = {};

	bvh.nodes = ARENA_ALLOC_ARRAY(arena, bvh_node_t, m_node_at);
	memcpy(bvh.nodes, m_nodes, sizeof(bvh_node_t) * m_node_at);

	bvh.triangles = ARENA_ALLOC_ARRAY(arena, triangle_t, m_tri_count);
	memcpy(bvh.triangles, m_tris, sizeof(triangle_t) * m_tri_count);

	bvh.triangle_indices = ARENA_ALLOC_ARRAY(arena, u32, m_tri_count);
	memcpy(bvh.triangle_indices, m_tri_indices, sizeof(u32) * m_tri_count);

	return bvh;
}

void bvh_builder_t::calc_node_min_max(bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max)
{
	node.aabb_min = glm::vec3(FLT_MAX);
	node.aabb_max = glm::vec3(-FLT_MAX);
	out_centroid_min = glm::vec3(FLT_MAX);
	out_centroid_max = glm::vec3(-FLT_MAX);

	for (u32 tri_idx = node.left_first; tri_idx < node.left_first + node.prim_count; ++tri_idx)
	{
		const triangle_t& triangle = m_tris[m_tri_indices[tri_idx]];

		glm::vec3 tri_min, tri_max;
		rt_util::get_triangle_min_max(triangle, tri_min, tri_max);

		node.aabb_min = glm::min(node.aabb_min, tri_min);
		node.aabb_max = glm::max(node.aabb_max, tri_max);

		const glm::vec3& tri_centroid = m_tri_centroids[m_tri_indices[tri_idx]];

		out_centroid_min = glm::min(out_centroid_min, tri_centroid);
		out_centroid_max = glm::max(out_centroid_max, tri_centroid);
	}
}

f32 bvh_builder_t::calc_node_cost(const bvh_node_t& node) const
{
	return node.prim_count * rt_util::get_aabb_volume(node.aabb_min, node.aabb_max);
}

void bvh_builder_t::subdivide_node(memory_arena_t* arena, bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max, u32 depth)
{
	u32 split_axis = 0;
	u32 split_pos = 0.0f;
	f32 split_cost = find_best_split_plane(arena, node, out_centroid_min, out_centroid_max, split_axis, split_pos);

	// If subdivide_single_prim is enabled in the build options, we always reduce the bvh_t down to a single primitive per leaf-node
	if (m_build_opts.subdivide_single_prim)
	{
		if (node.prim_count == 1)
			return;
	}
	// Otherwise we compare the cost of doing the split to the parent node cost, and terminate if a split is not worth it
	else
	{
		f32 parent_node_cost = calc_node_cost(node);
		if (split_cost >= parent_node_cost)
			return;
	}

	// indices to the first and last triangle indices in this node
	i32 i = node.left_first;
	i32 j = i + node.prim_count - 1;
	f32 bin_scale = m_build_opts.interval_count / (out_centroid_max[split_axis] - out_centroid_min[split_axis]);

	// This will sort the triangles along the axis and split position
	while (i <= j)
	{
		const glm::vec3& tri_centroid = m_tri_centroids[m_tri_indices[i]];

		i32 bin_idx = glm::min((i32)m_build_opts.interval_count - 1,
			(i32)((tri_centroid[split_axis] - out_centroid_min[split_axis]) * bin_scale));

		if (bin_idx < split_pos)
			i++;
		else
			std::swap(m_tri_indices[i], m_tri_indices[j--]);
	}

	// Determine how many nodes are on the left side of the split axis and position
	u32 prim_count_left = i - node.left_first;
	// If there is no or all primitives on the left side, we can stop splitting entirely
	if (prim_count_left == 0 || prim_count_left == node.prim_count)
		return;

	// Create two child nodes (left & right), and set their triangle start indices and count
	u32 left_child_node_idx = m_node_at++;
	bvh_node_t& left_child_node = m_nodes[left_child_node_idx];
	left_child_node.left_first = node.left_first;
	left_child_node.prim_count = prim_count_left;

	u32 right_child_node_idx = m_node_at++;
	bvh_node_t& right_child_node = m_nodes[right_child_node_idx];
	right_child_node.left_first = i;
	right_child_node.prim_count = node.prim_count - prim_count_left;

	// The current node we just split now becomes a parent node,
	// meaning it contains no primitives but points to the left and right nodes
	node.left_first = left_child_node_idx;
	node.prim_count = 0;

	// Calculate min/max and subdivide left child node
	calc_node_min_max(left_child_node, out_centroid_min, out_centroid_max);
	subdivide_node(arena, left_child_node, out_centroid_min, out_centroid_max, depth + 1);

	// Calculate min/max and subdivide right child node
	calc_node_min_max(right_child_node, out_centroid_min, out_centroid_max);
	subdivide_node(arena, right_child_node, out_centroid_min, out_centroid_max, depth + 1);
}

f32 bvh_builder_t::find_best_split_plane(memory_arena_t* arena, bvh_node_t& node, const glm::vec3& centroid_min, const glm::vec3& centroid_max, u32& out_axis, u32& out_split_pos)
{
	f32 cheapest_split_cost = FLT_MAX;

	for (u32 axis_idx = 0; axis_idx < 3; ++axis_idx)
	{
		f32 bounds_min = centroid_min[axis_idx], bounds_max = centroid_max[axis_idx];
		if (bounds_min == bounds_max)
			continue;

		u32 bin_count = m_build_opts.interval_count;
		bvh_bin_t* bvh_bins = ARENA_ALLOC_ARRAY_ZERO(arena, bvh_bin_t, bin_count);

		for (u32 i = 0; i < bin_count; ++i)
		{
			bvh_bin_t* bvh_bin = &bvh_bins[i];

			bvh_bin->aabb_min = glm::vec3(FLT_MAX);
			bvh_bin->aabb_max = glm::vec3(-FLT_MAX);
		}

		f32 bin_scale = bin_count / (bounds_max - bounds_min);

		// Grow the bins
		for (u32 tri_idx = node.left_first; tri_idx < node.left_first + node.prim_count; ++tri_idx)
		{
			const triangle_t& triangle = m_tris[m_tri_indices[tri_idx]];
			const glm::vec3& tri_centroid = m_tri_centroids[m_tri_indices[tri_idx]];

			i32 bin_idx = glm::min((i32)bin_count - 1,
				(i32)((tri_centroid[axis_idx] - bounds_min) * bin_scale));

			bvh_bin_t* bvh_bin = &bvh_bins[bin_idx];

			bvh_bin->prim_count++;
			rt_util::grow_aabb(bvh_bin->aabb_min, bvh_bin->aabb_max, triangle.p0);
			rt_util::grow_aabb(bvh_bin->aabb_min, bvh_bin->aabb_max, triangle.p1);
			rt_util::grow_aabb(bvh_bin->aabb_min, bvh_bin->aabb_max, triangle.p2);
		}

		// Get all necessary data for the planes between the bins
		f32* left_area = ARENA_ALLOC_ARRAY_ZERO(arena, f32, bin_count - 1);
		f32* right_area = ARENA_ALLOC_ARRAY_ZERO(arena, f32, bin_count - 1);

		glm::vec3 left_aabb_min(FLT_MAX), left_aabb_max(-FLT_MAX);
		glm::vec3 right_aabb_min(FLT_MAX), right_aabb_max(-FLT_MAX);

		u32 left_sum = 0, right_sum = 0;

		for (u32 bin_idx = 0; bin_idx < bin_count - 1; ++bin_idx)
		{
			const bvh_bin_t* left_bin = &bvh_bins[bin_idx];

			left_sum += left_bin->prim_count;
			rt_util::grow_aabb(left_aabb_min, left_aabb_max, left_bin->aabb_min, left_bin->aabb_max);
			left_area[bin_idx] = left_sum * rt_util::get_aabb_volume(left_aabb_min, left_aabb_max);

			const bvh_bin_t* right_bin = &bvh_bins[bin_count - 1 - bin_idx];
			right_sum += right_bin->prim_count;
			rt_util::grow_aabb(right_aabb_min, right_aabb_max, right_bin->aabb_min, right_bin->aabb_max);
			right_area[bin_count - 2 - bin_idx] = right_sum * rt_util::get_aabb_volume(right_aabb_min, right_aabb_max);
		}

		// Evaluate SAH cost for each plane
		bin_scale = (bounds_max - bounds_min) / bin_count;

		for (u32 bin_idx = 0; bin_idx < bin_count - 1; ++bin_idx)
		{
			f32 plane_split_cost = left_area[bin_idx] + right_area[bin_idx];

			if (plane_split_cost < cheapest_split_cost)
			{
				out_axis = axis_idx;
				out_split_pos = bin_idx + 1;
				cheapest_split_cost = plane_split_cost;
			}
		}
	}

	return cheapest_split_cost;
}