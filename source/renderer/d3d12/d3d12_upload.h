#pragma once
#include "d3d12_include.h"
#include"core/allocators/ring_alloc.h"

namespace d3d12
{

	struct upload_alloc_t
	{
		ID3D12Resource* d3d_resource;
		ID3D12CommandAllocator* d3d_command_allocator;
		ID3D12GraphicsCommandList6* d3d_command_list;
		uint64_t fence_value;
		void* ptr;

		ring_buffer_alloc_t ring_buffer_alloc;
	};

	namespace upload
	{

		void init(uint64_t capacity);
		void exit();

		// If the requested number of bytes is bigger than the upload buffer, the caller needs to handle uploading in multiple chunks instead
		upload_alloc_t& begin(uint64_t byte_count, uint64_t align = 0);
		uint64_t end(upload_alloc_t& alloc);
		void flush();

	}

}
