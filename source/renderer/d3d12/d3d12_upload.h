#pragma once
#include "d3d12_include.h"
#include"core/allocators/ring_alloc.h"

#include "core/api_types.h"

namespace d3d12
{

	inline constexpr uint32_t UPLOAD_BUFFER_DEFAULT_MAX_SUBMISSIONS = 32u;
	inline constexpr uint64_t UPLOAD_BUFFER_DEFAULT_CAPACITY = MB(64);

	struct upload_alloc_t
	{
		ID3D12Resource* d3d_resource;
		ID3D12CommandAllocator* d3d_command_allocator;
		ID3D12GraphicsCommandList6* d3d_command_list;
		uint64_t fence_value;
		void* ptr;

		ring_buffer_alloc_t ring_buffer_alloc;
	};

	struct upload_context_t
	{
		ID3D12Resource* buffer;
		void* buffer_ptr;

		ID3D12CommandQueue* command_queue_copy;
		ID3D12Fence* fence;
		uint64_t fence_value;

		upload_alloc_t allocations[UPLOAD_BUFFER_DEFAULT_MAX_SUBMISSIONS];
		uint32_t alloc_head;
		uint32_t alloc_tail;

		ring_buffer_t ring_buffer;
		mutex_t mutex;
	};

	void init_upload_context(uint64_t capacity);
	void destroy_upload_context();

	// If the requested number of bytes is bigger than the upload buffer, the caller needs to handle uploading in multiple chunks instead
	upload_alloc_t& begin_upload(uint64_t byte_count, uint64_t align = 0);
	uint64_t end_upload(upload_alloc_t& alloc);
	void flush_uploads();

}
