#include "pch.h"
#include "ring_alloc.h"
#include "renderer/d3d12/d3d12_backend.h"
#include "renderer/d3d12/d3d12_resource.h"
#include "core/thread.h"

namespace ring_buffer
{

	void init(ring_buffer_t& ring_buffer, u64 capacity)
	{
		ASSERT_MSG(IS_POW2(capacity), "Ring buffer capacity must be a power of 2");

		ring_buffer.capacity = capacity;
		ring_buffer.mask = capacity - 1;
		ring_buffer.head = 0;
		ring_buffer.tail = 0;
	}

	void destroy(ring_buffer_t& ring_buffer)
	{
		ring_buffer.capacity = 0;
		ring_buffer.mask = 0;
		ring_buffer.head = 0;
		ring_buffer.tail = 0;
	}

	b8 alloc(ring_buffer_t& ring_buffer, u64 byte_count, u64 align, ring_buffer_alloc_t& out_alloc)
	{
		align = MAX(align, 1);
		// Caller needs to handle ring buffer allocations that might be smaller than requested
		// Do this instead of potentially resizing the ring buffer
		byte_count = MIN(byte_count, ring_buffer.capacity);

		u64 aligned_head = ALIGN_UP_POW2(ring_buffer.head, align);

		u64 alloc_begin = (aligned_head) & ring_buffer.mask;
		u64 alloc_end = (aligned_head + byte_count - 1) & ring_buffer.mask;

		if (alloc_begin >= alloc_end)
		{
			aligned_head = ALIGN_UP_POW2(ring_buffer.head, ring_buffer.capacity);
			if (ring_buffer.head == ring_buffer.tail)
			{
				ring_buffer.tail = ALIGN_UP_POW2(ring_buffer.tail, ring_buffer.capacity);
			}

			alloc_begin = (aligned_head) & ring_buffer.mask;
			alloc_end = (aligned_head + byte_count - 1) & ring_buffer.mask;
		}

		ASSERT_MSG(alloc_begin < alloc_end, "Ring buffer allocation requires wrapping but failed");

		// Determine how much space is available based on the aligned head and the ring buffer tail
		u64 capacity_used = aligned_head - ring_buffer.tail;
		u64 capacity_free = ring_buffer.capacity - capacity_used;

		if (capacity_free >= byte_count)
		{
			out_alloc.byte_offset = alloc_begin;
			out_alloc.byte_size = byte_count;
			out_alloc.head = aligned_head + out_alloc.byte_size;

			ring_buffer.head = out_alloc.head;
			return true;
		}

		return false;
	}

	void free(ring_buffer_t& ring_buffer, const ring_buffer_alloc_t& alloc)
	{
		ring_buffer.tail = alloc.head;
	}

}
