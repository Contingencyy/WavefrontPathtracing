#include "pch.h"
#include "tlas_builder.h"
#include "bvh_builder.h"
#include "renderer/raytracing_utils.h"

void tlas_builder_t::build(memory_arena_t* arena, bvh_instance_t* bvh_instances, u32 bvh_instance_count)
{
	m_instance_count = bvh_instance_count;
	m_instances = bvh_instances;

	m_node_count = m_instance_count * 2;
	m_nodes = ARENA_ALLOC_ARRAY_ZERO(arena, tlas_node_t, m_node_count);

	u32 node_idx[256] = {};
	u32 node_indices = m_instance_count;
	m_node_at = 1;

	for (size_t i = 0; i < m_instance_count; ++i)
	{
		const aabb_t& blas_bb = m_instances[i].aabb_world;

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

tlas_t tlas_builder_t::extract(memory_arena_t* arena) const
{
	tlas_t tlas = {};

	tlas.nodes = ARENA_ALLOC_ARRAY(arena, tlas_node_t, m_node_at);
	memcpy(tlas.nodes, m_nodes, sizeof(tlas_node_t) * m_node_at);

	tlas.instances = ARENA_ALLOC_ARRAY(arena, bvh_instance_t, m_instance_count);
	memcpy(tlas.instances, m_instances, sizeof(bvh_instance_t) * m_instance_count);

	return tlas;
}

u32 tlas_builder_t::find_best_match(u32 A, const u32* indices, u32 index_count)
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
