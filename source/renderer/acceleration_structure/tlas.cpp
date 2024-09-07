#include "pch.h"
#include "tLAS.h"
#include "bvh_instance.h"
#include "renderer/raytracing_utils.h"

void tlas_t::build(memory_arena_t* arena, bvh_instance_t* blas, u32 blas_count)
{
	m_blas_instance_count = blas_count;
	m_blas_instances = blas;

	m_node_count = m_blas_instance_count * 2;
	m_nodes = ARENA_ALLOC_ARRAY_ZERO(arena, tlas_node_t, m_node_count);

	u32 node_idx[256] = {};
	u32 node_indices = blas_count;
	m_node_at = 1;

	for (size_t i = 0; i < blas_count; ++i)
	{
		const aabb_t& blas_bb = m_blas_instances[i].get_world_space_aabb();

		node_idx[i] = m_node_at;

		m_nodes[m_node_at].aabb_min = blas_bb.pmin;
		m_nodes[m_node_at].aabb_max = blas_bb.pmax;
		m_nodes[m_node_at].blas_instance_index = i;
		m_nodes[m_node_at].left_right = 0;

		m_node_at++;
	}

	// Apply agglomerative clustering to build the tlas_t
	u32 A = 0;
	u32 B = find_best_match(A, node_idx, node_indices);

	while (node_indices > 1)
	{
		u32 C = find_best_match(B, node_idx, node_indices);

		// B is the best match for A, so now we check if the best match for B is also A, and if true, combine
		if (A == C)
		{
			const u32 node_idx_A = node_idx[A];
			const u32 node_idx_B = node_idx[B];

			const tlas_node_t& node_A = m_nodes[node_idx_A];
			const tlas_node_t& node_B = m_nodes[node_idx_B];
			tlas_node_t& new_node = m_nodes[m_node_at];

			new_node.left_right = node_idx_A + (node_idx_B << 16);
			new_node.aabb_min = glm::min(node_A.aabb_min, node_B.aabb_min);
			new_node.aabb_max = glm::max(node_A.aabb_max, node_B.aabb_max);

			node_idx[A] = m_node_at++;
			node_idx[B] = node_idx[node_indices - 1];

			B = find_best_match(A, node_idx, --node_indices);
		}
		else
		{
			A = B;
			B = C;
		}
	}

	m_nodes[0] = m_nodes[node_idx[A]];
}

void tlas_t::trace_ray(ray_t& ray_t, hit_result_t& hit_result_t) const
{
	const tlas_node_t* node = &m_nodes[0];
	// Check if we miss the tlas_t entirely
	if (rt_util::intersect_sse(node->aabb_min4, node->aabb_max4, ray_t) == FLT_MAX)
		return;

	ray_t.bvh_depth++;
	const tlas_node_t* stack[64] = {};
	u32 stack_at = 0;

	while (true)
	{
		// The current node is a leaf node, so it contains a bvh_t which we need to trace against
		if (node->is_leaf())
		{
			const bvh_instance_t& blas_instance = m_blas_instances[node->blas_instance_index];

			// Trace object-space ray against object-space bvh_t
			const triangle_t* hit_triangle = blas_instance.trace_ray(ray_t, hit_result_t);

			// If we have hit a triangle, update the hit result
			if (hit_triangle)
			{
				// Hit t, bary and prim_idx are written in bvh_t::trace_ray, Hit pos and normal in bvh_instance_t::trace_ray
				hit_result_t.instance_idx = node->blas_instance_index;
			}

			if (stack_at == 0)
				break;
			else
				node = stack[--stack_at];
			continue;
		}

		// If the current node is not a leaf node, keep traversing the left and right child nodes
		const tlas_node_t* left_child_node = &m_nodes[node->left_right >> 16];
		const tlas_node_t* right_child_node = &m_nodes[node->left_right & 0x0000FFFF];

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
				node = stack[--stack_at];
		}
		// We have intersected with either the left or right child node, so check the closest node first
		// and push the other one on the stack, if we have hit that one as well
		else
		{
			ray_t.bvh_depth++;

			node = left_child_node;
			if (right_dist != FLT_MAX)
				stack[stack_at++] = right_child_node;
		}
	}
}

u32 tlas_t::find_best_match(u32 A, const u32* indices, u32 index_count)
{
	f32 smallest_area = FLT_MAX;
	u32 best_fit_idx = ~0u;

	// find the combination of node at index A and any other node that yields the smallest bounding box
	for (size_t B = 0; B < index_count; ++B)
	{
		if (B != A)
		{
			glm::vec3 bmax = glm::max(m_nodes[indices[A]].aabb_min, m_nodes[indices[B]].aabb_min);
			glm::vec3 bmin = glm::max(m_nodes[indices[A]].aabb_max, m_nodes[indices[B]].aabb_max);
			glm::vec3 extent = bmax - bmin;

			f32 area = rt_util::get_aabb_volume(bmin, bmax);
			if (area < smallest_area)
			{
				smallest_area = area;
				best_fit_idx = B;
			}
		}
	}

	return best_fit_idx;
}

b8 tlas_t::tlas_node_t::is_leaf() const
{
	return left_right == 0;
}
