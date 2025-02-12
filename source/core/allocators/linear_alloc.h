#pragma once

struct linear_alloc_t
{
	uint64_t at;
	uint64_t capacity;
};

namespace linear_alloc
{

	void init(linear_alloc_t& allocator, uint64_t capacity);
	void destroy(linear_alloc_t& allocator);

	bool alloc(linear_alloc_t& allocator, uint64_t& out_offset, uint64_t byte_count, uint64_t align = 0);
	void free(linear_alloc_t& allocator, uint64_t marker);
	void reset(linear_alloc_t& allocator);

}
