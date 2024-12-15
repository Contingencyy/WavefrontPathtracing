#pragma once

struct ring_buffer_alloc_t
{
	u64 byte_offset;
	u64 byte_size;
	u64 head;
};

struct ring_buffer_t
{
	u64 capacity;
	u64 mask;

	u64 head;
	u64 tail;
};

namespace ring_buffer
{

	void init(ring_buffer_t& ring_buffer, u64 capacity);
	void destroy(ring_buffer_t& ring_buffer);

	// The size of the allocation is not guaranteed to be the requested size, since the requested size might be bigger than the buffer.
	// Calling code needs to handle uploading in chunks if that occurs. This function might block if the ring buffer is currently full.
	b8 alloc(ring_buffer_t& ring_buffer, u64 byte_count, u64 align, ring_buffer_alloc_t& out_alloc);
	void free(ring_buffer_t& ring_buffer, const ring_buffer_alloc_t& alloc);

}
