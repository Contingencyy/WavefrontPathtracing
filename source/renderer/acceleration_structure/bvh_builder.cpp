#include "pch.h"
#include "bvh_builder.h"
#include "as_util.h"

#include "renderer/renderer_fwd.h"

void bvh_builder_t::build(memory_arena_t* arena, const build_args_t& build_args)
{
	m_build_opts = build_args.options;

	m_triangle_count = build_args.index_count / 3;
	m_triangles = ARENA_ALLOC_ARRAY_ZERO(arena, bvh_triangle_t, m_triangle_count);
	m_triangle_indices = ARENA_ALLOC_ARRAY_ZERO(arena, uint32_t, m_triangle_count);
	m_triangle_centroids = ARENA_ALLOC_ARRAY_ZERO(arena, glm::vec3, m_triangle_count);

	m_node_count = m_triangle_count * 2;
	m_nodes = ARENA_ALLOC_ARRAY_ZERO(arena, bvh_node_t, m_node_count);
	m_node_at = 0;

	// Make bvh triangles from vertices and indices
	for (uint32_t tri_idx = 0, i = 0; tri_idx < m_triangle_count; ++tri_idx, i += 3)
	{
		m_triangles[tri_idx].p0 = build_args.vertices[build_args.indices[i]].position;
		m_triangles[tri_idx].p1 = build_args.vertices[build_args.indices[i + 1]].position;
		m_triangles[tri_idx].p2 = build_args.vertices[build_args.indices[i + 2]].position;
	}

	// Fill all triangle indices with their default value
	for (uint32_t i = 0; i < m_triangle_count; ++i)
	{
		m_triangle_indices[i] = i;
	}

	// Calculate all triangle centroids
	for (size_t i = 0; i < m_triangle_count; ++i)
	{
		m_triangle_centroids[i] = get_triangle_centroid(m_triangles[i]);
	}

	// Set the first node to be the root node
	bvh_node_t& root_node = m_nodes[m_node_at];
	root_node.left_first = 0;
	root_node.prim_count = m_triangle_count;

	// Skip over m_BVHNodes[1] for cache alignment
	m_node_at = 2;

	glm::vec3 node_centroid_min, node_centroid_max;
	calc_node_min_max(root_node, node_centroid_min, node_centroid_max);
	subdivide_node(arena, root_node, node_centroid_min, node_centroid_max, 0);
}

void bvh_builder_t::extract(memory_arena_t* arena, bvh_t& out_bvh, uint64_t& out_bvh_byte_size) const
{
	uint32_t header_size = sizeof(bvh_header_t);
	uint32_t nodes_byte_size = sizeof(bvh_node_t) * m_node_at;
	uint32_t triangles_byte_size = sizeof(bvh_triangle_t) * m_triangle_count;
	uint32_t triangle_indices_byte_size = sizeof(uint32_t) * m_triangle_count;

	out_bvh_byte_size = /*header_size + */nodes_byte_size + triangles_byte_size + triangle_indices_byte_size;
	out_bvh.data = ARENA_ALLOC(arena, out_bvh_byte_size, alignof(bvh_t));

	out_bvh.header.nodes_offset = header_size;
	out_bvh.header.triangles_offset = header_size + nodes_byte_size;
	out_bvh.header.indices_offset = header_size + nodes_byte_size + triangles_byte_size;

	memcpy(PTR_OFFSET(out_bvh.data, 0), m_nodes, nodes_byte_size);
	memcpy(PTR_OFFSET(out_bvh.data, nodes_byte_size), m_triangles, triangles_byte_size);
	memcpy(PTR_OFFSET(out_bvh.data, nodes_byte_size + triangles_byte_size), m_triangle_indices, triangle_indices_byte_size);
}

void bvh_builder_t::calc_node_min_max(bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max)
{
	node.aabb_min = glm::vec3(FLT_MAX);
	node.aabb_max = glm::vec3(-FLT_MAX);
	out_centroid_min = glm::vec3(FLT_MAX);
	out_centroid_max = glm::vec3(-FLT_MAX);

	for (uint32_t tri_idx = node.left_first; tri_idx < node.left_first + node.prim_count; ++tri_idx)
	{
		const bvh_triangle_t& triangle = m_triangles[m_triangle_indices[tri_idx]];

		glm::vec3 tri_min, tri_max;
		get_triangle_min_max(triangle, tri_min, tri_max);

		node.aabb_min = glm::min(node.aabb_min, tri_min);
		node.aabb_max = glm::max(node.aabb_max, tri_max);

		const glm::vec3& tri_centroid = m_triangle_centroids[m_triangle_indices[tri_idx]];

		out_centroid_min = glm::min(out_centroid_min, tri_centroid);
		out_centroid_max = glm::max(out_centroid_max, tri_centroid);
	}
}

float bvh_builder_t::calc_node_cost(const bvh_node_t& node) const
{
	return node.prim_count * as_util::get_aabb_volume(node.aabb_min, node.aabb_max);
}

void bvh_builder_t::subdivide_node(memory_arena_t* arena, bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max, uint32_t depth)
{
	uint32_t split_axis = 0;
	uint32_t split_pos = 0.0f;
	float split_cost = find_best_split_plane(arena, node, out_centroid_min, out_centroid_max, split_axis, split_pos);

	// If subdivide_single_prim is enabled in the build options, we always reduce the bvh_t down to a single primitive per leaf-node
	if (m_build_opts.subdivide_single_prim)
	{
		if (node.prim_count == 1)
			return;
	}
	// Otherwise we compare the cost of doing the split to the parent node cost, and terminate if a split is not worth it
	else
	{
		float parent_node_cost = calc_node_cost(node);
		if (split_cost >= parent_node_cost)
			return;
	}

	// indices to the first and last triangle indices in this node
	int32_t i = node.left_first;
	int32_t j = i + node.prim_count - 1;
	float bin_scale = m_build_opts.interval_count / (out_centroid_max[split_axis] - out_centroid_min[split_axis]);

	// This will sort the triangles along the axis and split position
	while (i <= j)
	{
		const glm::vec3& tri_centroid = m_triangle_centroids[m_triangle_indices[i]];

		int32_t bin_idx = glm::min((int32_t)m_build_opts.interval_count - 1,
			(int32_t)((tri_centroid[split_axis] - out_centroid_min[split_axis]) * bin_scale));

		if (bin_idx < split_pos)
			i++;
		else
			std::swap(m_triangle_indices[i], m_triangle_indices[j--]);
	}

	// Determine how many nodes are on the left side of the split axis and position
	uint32_t prim_count_left = i - node.left_first;
	// If there is no or all primitives on the left side, we can stop splitting entirely
	if (prim_count_left == 0 || prim_count_left == node.prim_count)
		return;

	// Create two child nodes (left & right), and set their triangle start indices and count
	uint32_t left_child_node_idx = m_node_at++;
	bvh_node_t& left_child_node = m_nodes[left_child_node_idx];
	left_child_node.left_first = node.left_first;
	left_child_node.prim_count = prim_count_left;

	uint32_t right_child_node_idx = m_node_at++;
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

float bvh_builder_t::find_best_split_plane(memory_arena_t* arena, bvh_node_t& node, const glm::vec3& centroid_min, const glm::vec3& centroid_max, uint32_t& out_axis, uint32_t& out_split_pos)
{
	float cheapest_split_cost = FLT_MAX;

	for (uint32_t axis_idx = 0; axis_idx < 3; ++axis_idx)
	{
		float bounds_min = centroid_min[axis_idx], bounds_max = centroid_max[axis_idx];
		if (bounds_min == bounds_max)
			continue;

		uint32_t bin_count = m_build_opts.interval_count;
		bvh_bin_t* bvh_bins = ARENA_ALLOC_ARRAY_ZERO(arena, bvh_bin_t, bin_count);

		for (uint32_t i = 0; i < bin_count; ++i)
		{
			bvh_bin_t* bvh_bin = &bvh_bins[i];

			bvh_bin->aabb_min = glm::vec3(FLT_MAX);
			bvh_bin->aabb_max = glm::vec3(-FLT_MAX);
		}

		float bin_scale = bin_count / (bounds_max - bounds_min);

		// Grow the bins
		for (uint32_t tri_idx = node.left_first; tri_idx < node.left_first + node.prim_count; ++tri_idx)
		{
			const bvh_triangle_t& triangle = m_triangles[m_triangle_indices[tri_idx]];
			const glm::vec3& tri_centroid = m_triangle_centroids[m_triangle_indices[tri_idx]];

			int32_t bin_idx = glm::min((int32_t)bin_count - 1,
				(int32_t)((tri_centroid[axis_idx] - bounds_min) * bin_scale));

			bvh_bin_t* bvh_bin = &bvh_bins[bin_idx];

			bvh_bin->prim_count++;
			as_util::grow_aabb(bvh_bin->aabb_min, bvh_bin->aabb_max, triangle.p0);
			as_util::grow_aabb(bvh_bin->aabb_min, bvh_bin->aabb_max, triangle.p1);
			as_util::grow_aabb(bvh_bin->aabb_min, bvh_bin->aabb_max, triangle.p2);
		}

		// Get all necessary data for the planes between the bins
		float* left_area = ARENA_ALLOC_ARRAY_ZERO(arena, float, bin_count - 1);
		float* right_area = ARENA_ALLOC_ARRAY_ZERO(arena, float, bin_count - 1);

		glm::vec3 left_aabb_min(FLT_MAX), left_aabb_max(-FLT_MAX);
		glm::vec3 right_aabb_min(FLT_MAX), right_aabb_max(-FLT_MAX);

		uint32_t left_sum = 0, right_sum = 0;

		for (uint32_t bin_idx = 0; bin_idx < bin_count - 1; ++bin_idx)
		{
			const bvh_bin_t* left_bin = &bvh_bins[bin_idx];

			left_sum += left_bin->prim_count;
			as_util::grow_aabb(left_aabb_min, left_aabb_max, left_bin->aabb_min, left_bin->aabb_max);
			left_area[bin_idx] = left_sum * as_util::get_aabb_volume(left_aabb_min, left_aabb_max);

			const bvh_bin_t* right_bin = &bvh_bins[bin_count - 1 - bin_idx];
			right_sum += right_bin->prim_count;
			as_util::grow_aabb(right_aabb_min, right_aabb_max, right_bin->aabb_min, right_bin->aabb_max);
			right_area[bin_count - 2 - bin_idx] = right_sum * as_util::get_aabb_volume(right_aabb_min, right_aabb_max);
		}

		// Evaluate SAH cost for each plane
		bin_scale = (bounds_max - bounds_min) / bin_count;

		for (uint32_t bin_idx = 0; bin_idx < bin_count - 1; ++bin_idx)
		{
			float plane_split_cost = left_area[bin_idx] + right_area[bin_idx];

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

glm::vec3 bvh_builder_t::get_triangle_centroid(const bvh_triangle_t& triangle)
{
	return (triangle.p0 + triangle.p1 + triangle.p2) * 0.3333f;
}

void bvh_builder_t::get_triangle_min_max(const bvh_triangle_t& triangle, glm::vec3& out_min, glm::vec3& out_max)
{
	out_min = glm::min(triangle.p0, triangle.p1);
	out_max = glm::max(triangle.p0, triangle.p1);

	out_min = glm::min(out_min, triangle.p2);
	out_max = glm::max(out_max, triangle.p2);
}
