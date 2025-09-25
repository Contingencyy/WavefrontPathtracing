#include "core/assertion.h"
#include "linear_alloc.h"

namespace linear_alloc
{

	void init(linear_alloc_t& allocator, uint64_t capacity)
	{
		allocator.at = 0;
		allocator.capacity = capacity;
	}

	void destroy(linear_alloc_t& allocator)
	{
		allocator.at = 0;
		allocator.capacity = 0;
	}

	bool alloc(linear_alloc_t& allocator, uint64_t& out_offset, uint64_t byte_count, uint64_t align)
	{
		align = MAX(align, 1);
		uint64_t aligned_at = ALIGN_UP_POW2(allocator.at, align);
		uint64_t bytes_free = allocator.capacity - aligned_at;

		if (bytes_free >= byte_count)
		{
			allocator.at = aligned_at + byte_count;
			out_offset = aligned_at;

			return true;
		}

		return false;
	}

	void free(linear_alloc_t& allocator, uint64_t marker)
	{
		ASSERT(marker <= allocator.capacity);
		allocator.at = marker;
	}

	void reset(linear_alloc_t& allocator)
	{
		allocator.at = 0;
	}

}
