#pragma once
#include "renderer/shaders/shared.hlsl.h"

struct memory_arena_t;
class bvh_instance_t;

struct tlas_t
{
	tlas_header_t header;
	void* data;
};

class tlas_builder_t
{
public:
	void build(memory_arena_t* arena, bvh_instance_t* bvh_instances, u32 bvh_instance_count);
	void extract(memory_arena_t* arena, tlas_t& out_tlas, u64& out_tlas_byte_size) const;

private:
	u32 find_best_match(u32 A, const u32* indices, u32 index_count);

private:
	u32 m_node_count;
	u32 m_node_at = 0;
	tlas_node_t* m_nodes;

	u32 m_instance_count;
	bvh_instance_t* m_instances;

};
