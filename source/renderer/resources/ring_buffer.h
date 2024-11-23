#pragma once
#include "renderer/d3d12/d3d12_include.h"

#define RING_BUFFER_MAX_ALLOCATIONS 32

struct upload_alloc_t
{
	ID3D12Resource* d3d_resource;

	ID3D12CommandAllocator* d3d_command_allocator;
	ID3D12GraphicsCommandList6* d3d_command_list;

	u64 fence_value;
	u64 head;
	
	u64 byte_offset;
	u64 byte_size;
	u8* ptr;
};

struct ring_buffer_t
{
	ID3D12Resource* d3d_resource;

	ID3D12CommandQueue* d3d_command_queue;
	ID3D12Fence* d3d_fence;
	u64 fence_value_at;

	u64 capacity;
	u64 head;
	u64 tail;
	u8* ptr_base;

	upload_alloc_t allocations[RING_BUFFER_MAX_ALLOCATIONS];
	u32 alloc_head;
	u32 alloc_tail;

	mutex_t mutex;
};

namespace ring_buffer
{

	void init(ring_buffer_t& ring_buffer, const wchar_t* name, u64 capacity);
	void destroy(ring_buffer_t& ring_buffer);

	// THREAD_SAFE. The size of the allocation is not guaranteed to be the requested size, since the requested size might be bigger than the buffer.
	// Calling code needs to handle uploading in chunks if that occurs. This function might block if the ring buffer is currently full.
	upload_alloc_t* alloc(ring_buffer_t& ring_buffer, u64 num_requested_bytes, u64 align);
	// TODO: Implement write_to_upload for range checks?
	// THREAD_SAFE.
	u64 submit(ring_buffer_t& ring_buffer, upload_alloc_t* upload_alloc);
	void flush(ring_buffer_t& ring_buffer);

}
