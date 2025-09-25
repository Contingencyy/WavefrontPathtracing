#include "tlas_builder.h"
#include "bvh_builder.h"
#include "as_util.h"
#include "core/memory/memory_arena.h"

void tlas_builder_t::build(memory_arena_t& arena, bvh_instance_t* bvh_instances, uint32_t bvh_instance_count)
{
	m_instance_count = bvh_instance_count;
	m_instances = bvh_instances;

	m_node_count = m_instance_count * 2;
	m_nodes = ARENA_ALLOC_ARRAY_ZERO(arena, tlas_node_t, m_node_count);

	uint32_t node_idx[256] = {};
	uint32_t node_indices = m_instance_count;
	m_node_at = 1;

	for (size_t i = 0; i < m_instance_count; ++i)
	{
		const glm::vec3& blas_min = m_instances[i].aabb_min;
		const glm::vec3& blas_max = m_instances[i].aabb_max;

		node_idx[i] = m_node_at;

		m_nodes[m_node_at].aabb_min = blas_min;
		m_nodes[m_node_at].aabb_max = blas_max;
		m_nodes[m_node_at].instance_idx = i;
		m_nodes[m_node_at].left_right = 0;

		m_node_at++;
	}

	// Apply agglomerative clustering to build the tlas
	uint32_t A = 0;
	uint32_t B = find_best_match(A, node_idx, node_indices);

	while (node_indices > 1)
	{
		uint32_t C = find_best_match(B, node_idx, node_indices);

		// B is the best match for A, so now we check if the best match for B is also A, and if true, combine
		if (A == C)
		{
			const uint32_t node_idx_A = node_idx[A];
			const uint32_t node_idx_B = node_idx[B];

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

void tlas_builder_t::extract(memory_arena_t& arena, tlas_t& out_tlas, uint64_t& out_tlas_byte_size) const
{
	uint32_t header_size = sizeof(tlas_header_t);
	uint32_t nodes_byte_size = sizeof(tlas_node_t) * m_node_at;
	uint32_t instances_byte_size = sizeof(bvh_instance_t) * m_instance_count;

	out_tlas_byte_size = /*header_size + */nodes_byte_size + instances_byte_size;
	out_tlas.data = ARENA_ALLOC(arena, out_tlas_byte_size, alignof(tlas_t));
	
	out_tlas.header.nodes_offset = header_size;
	out_tlas.header.instances_offset = header_size + nodes_byte_size;

	memcpy(PTR_OFFSET(out_tlas.data, 0), m_nodes, nodes_byte_size);
	memcpy(PTR_OFFSET(out_tlas.data, nodes_byte_size), m_instances, instances_byte_size);
}

uint32_t tlas_builder_t::find_best_match(uint32_t A, const uint32_t* indices, uint32_t index_count)
{
	float smallest_area = FLT_MAX;
	uint32_t best_fit_idx = ~0u;

	// Find the combination of node at index A and any other node that yields the smallest bounding box
	for (size_t B = 0; B < index_count; ++B)
	{
		if (B != A)
		{
			glm::vec3 bmax = glm::max(m_nodes[indices[A]].aabb_min, m_nodes[indices[B]].aabb_min);
			glm::vec3 bmin = glm::max(m_nodes[indices[A]].aabb_max, m_nodes[indices[B]].aabb_max);
			glm::vec3 extent = bmax - bmin;

			float area = as_util::get_aabb_volume(bmin, bmax);
			if (area < smallest_area)
			{
				smallest_area = area;
				best_fit_idx = B;
			}
		}
	}

	return best_fit_idx;
}
