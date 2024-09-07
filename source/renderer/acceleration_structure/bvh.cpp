#include "pch.h"
#include "bvh.h"
#include "renderer/raytracing_utils.h"
#include "renderer/renderer_fwd.h"

void bvh_t::build(memory_arena_t* arena, vertex_t* vertices, u32 vertex_count, u32* indices, u32 index_count, const build_options_t& build_opts)
{
	m_build_opts = build_opts;
	
	m_tri_count = index_count / 3;
	m_tris = ARENA_ALLOC_ARRAY_ZERO(arena, triangle_t, m_tri_count);
	m_tri_indices = ARENA_ALLOC_ARRAY_ZERO(arena, u32, m_tri_count);
	m_tri_centroids = ARENA_ALLOC_ARRAY_ZERO(arena, glm::vec3, m_tri_count);

	m_node_count = m_tri_count * 2;
	m_nodes = ARENA_ALLOC_ARRAY_ZERO(arena, bvh_node_t, m_node_count);
	m_node_at = 0;

	// Make bvh_t triangles from vertices and indices
	for (u32 tri_idx = 0, i = 0; tri_idx < m_tri_count; ++tri_idx, i += 3)
	{
		m_tris[tri_idx].p0 = vertices[indices[i]].position;
		m_tris[tri_idx].p1 = vertices[indices[i + 1]].position;
		m_tris[tri_idx].p2 = vertices[indices[i + 2]].position;

		m_tris[tri_idx].n0 = vertices[indices[i]].normal;
		m_tris[tri_idx].n1 = vertices[indices[i + 1]].normal;
		m_tris[tri_idx].n2 = vertices[indices[i + 2]].normal;
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

const triangle_t* bvh_t::trace_ray(ray_t& ray_t, hit_result_t& hit_result_t) const
{
	const triangle_t* hit_triangle = nullptr;

	const bvh_node_t* node_curr = &m_nodes[0];
	const bvh_node_t* stack[64] = {};
	u32 stack_at = 0;

	while (true)
	{
		// The current node is a leaf node, so we need to check intersections with the primitives
		if (node_curr->prim_count > 0)
		{
			for (u32 tri_idx = node_curr->left_first; tri_idx < node_curr->left_first + node_curr->prim_count; ++tri_idx)
			{
				const triangle_t& triangle = m_tris[m_tri_indices[tri_idx]];
				b8 intersected = rt_util::intersect(triangle, ray_t, hit_result_t.t, hit_result_t.bary);

				if (intersected)
				{
					hit_result_t.prim_idx = tri_idx;
					hit_triangle = &m_tris[m_tri_indices[tri_idx]];
				}
			}

			if (stack_at == 0)
				break;
			else
				node_curr = stack[--stack_at];
			continue;
		}

		// If the current node is not a leaf node, keep traversing the left and right child nodes
		const bvh_node_t* left_child_node = &m_nodes[node_curr->left_first];
		const bvh_node_t* right_child_node = &m_nodes[node_curr->left_first + 1];

		// Determine distance to left and right child nodes, using SSE aabb_t intersections
		f32 left_dist = rt_util::intersect_sse(left_child_node->aabb_min4, left_child_node->aabb_max4, ray_t);
		f32 right_dist = rt_util::intersect_sse(right_child_node->aabb_min4, right_child_node->aabb_max4, ray_t);

		// Swap left and right child nodes so the closest is now the left child
		if (left_dist > right_dist)
		{
			std::swap(left_dist, right_dist);
			std::swap(left_child_node, right_child_node);
		}

		// If we have not intersected with the left and right child nodes, we can keep traversing the stack, or terminate if stack is empty
		if (left_dist == FLT_MAX)
		{
			if (stack_at == 0)
				break;
			else
				node_curr = stack[--stack_at];
		}
		// We have intersected with either the left or right child node, so check the closest node first
		// and push the other one on the stack, if we have hit that one as well
		else
		{
			ray_t.bvh_depth++;

			node_curr = left_child_node;
			if (right_dist != FLT_MAX)
				stack[stack_at++] = right_child_node;
		}
	}

	return hit_triangle;
}

aabb_t bvh_t::get_local_space_aabb() const
{
	return { m_nodes[0].aabb_min, m_nodes[0].aabb_max };
}

void bvh_t::calc_node_min_max(bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max)
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

f32 bvh_t::calc_node_cost(const bvh_node_t& node) const
{
	return node.prim_count * rt_util::get_aabb_volume(node.aabb_min, node.aabb_max);
}

void bvh_t::subdivide_node(memory_arena_t* arena, bvh_node_t& node, glm::vec3& out_centroid_min, glm::vec3& out_centroid_max, u32 depth)
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

f32 bvh_t::find_best_split_plane(memory_arena_t* arena, bvh_node_t& node, const glm::vec3& centroid_min, const glm::vec3& centroid_max, u32& out_axis, u32& out_split_pos)
{
	f32 cheapest_split_cost = FLT_MAX;

	ARENA_MEMORY_SCOPE(arena)
	{
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
	}

	return cheapest_split_cost;
}
