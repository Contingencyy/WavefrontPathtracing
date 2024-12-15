#include "pch.h"
#include "linear_alloc.h"

namespace linear_alloc
{

	void init(linear_alloc_t& allocator, u64 capacity)
	{
		allocator.at = 0;
		allocator.capacity = capacity;
	}

	void destroy(linear_alloc_t& allocator)
	{
		allocator.at = 0;
		allocator.capacity = 0;
	}

	b8 alloc(linear_alloc_t& allocator, u64& out_offset, u64 byte_count, u64 align)
	{
		align = MAX(align, 1);
		u64 aligned_at = ALIGN_UP_POW2(allocator.at, align);
		u64 bytes_free = allocator.capacity - aligned_at;

		if (bytes_free >= byte_count)
		{
			allocator.at = aligned_at + byte_count;
			out_offset = aligned_at;

			return true;
		}

		return false;
	}

	void free(linear_alloc_t& allocator, u64 marker)
	{
		ASSERT(marker <= allocator.capacity);
		allocator.at = marker;
	}

	void reset(linear_alloc_t& allocator)
	{
		allocator.at = 0;
	}

}
