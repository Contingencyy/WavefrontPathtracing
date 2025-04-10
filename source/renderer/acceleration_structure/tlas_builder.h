#pragma once
#include "renderer/shaders/shared.hlsl.h"

struct memory_arena_t;
struct bvh_instance_t;

struct tlas_t
{
	tlas_header_t header;
	void* data;
};

class tlas_builder_t
{
public:
	void build(memory_arena_t& arena, bvh_instance_t* bvh_instances, uint32_t bvh_instance_count);
	void extract(memory_arena_t& arena, tlas_t& out_tlas, uint64_t& out_tlas_byte_size) const;

private:
	uint32_t find_best_match(uint32_t A, const uint32_t* indices, uint32_t index_count);

private:
	uint32_t m_node_count;
	uint32_t m_node_at = 0;
	tlas_node_t* m_nodes;

	uint32_t m_instance_count;
	bvh_instance_t* m_instances;

};
