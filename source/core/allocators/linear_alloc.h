#pragma once

struct linear_alloc_t
{
	u64 at;
	u64 capacity;
};

namespace linear_alloc
{

	void init(linear_alloc_t& allocator, u64 capacity);
	void destroy(linear_alloc_t& allocator);

	b8 alloc(linear_alloc_t& allocator, u64& out_offset, u64 byte_count, u64 align = 0);
	void free(linear_alloc_t& allocator, u64 marker);
	void reset(linear_alloc_t& allocator);

}
