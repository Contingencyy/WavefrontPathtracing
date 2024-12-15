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
		u64 fence_value;
		void* ptr;

		ring_buffer_alloc_t ring_buffer_alloc;
	};

	namespace upload
	{

		void init(u64 capacity);
		void exit();

		upload_alloc_t& begin(u64 byte_count, u64 align = 0);
		u64 end(upload_alloc_t& alloc);
		void flush();

	}

}
