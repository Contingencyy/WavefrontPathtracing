#pragma once
#include "core/common.h"

struct ring_buffer_alloc_t
{
	uint64_t byte_offset;
	uint64_t byte_size;
	uint64_t head;
};

struct ring_buffer_t
{
	uint64_t capacity;
	uint64_t mask;

	uint64_t head;
	uint64_t tail;
};

namespace ring_buffer
{

	void init(ring_buffer_t& ring_buffer, uint64_t capacity);
	void destroy(ring_buffer_t& ring_buffer);

	// The size of the allocation is not guaranteed to be the requested size, since the requested size might be bigger than the buffer.
	// Calling code needs to handle uploading in chunks if that occurs. This function might block if the ring buffer is currently full.
	bool alloc(ring_buffer_t& ring_buffer, uint64_t byte_count, uint64_t align, ring_buffer_alloc_t& out_alloc);
	void free(ring_buffer_t& ring_buffer, const ring_buffer_alloc_t& alloc);

}
