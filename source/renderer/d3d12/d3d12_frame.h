#pragma once
#include "d3d12_include.h"
#include "d3d12_descriptor.h"

#include "core/allocators/linear_alloc.h"

namespace d3d12
{

	struct frame_context_t
	{
		ID3D12CommandAllocator* command_allocator;
		ID3D12GraphicsCommandList6* command_list;

		ID3D12Resource* backbuffer;
		descriptor_allocation_t backbuffer_rtv;
		uint64_t fence_value;

		ID3D12Resource* per_frame_resource;
		linear_alloc_t per_frame_allocator;
		uint8_t* per_frame_ptr;
	};

	struct frame_alloc_t
	{
		ID3D12Resource* resource;
		uint64_t byte_offset;
		uint64_t byte_count;

		uint8_t* ptr;
	};

	namespace frame
	{

		void init(uint64_t resource_capacity, const DXGI_SWAP_CHAIN_DESC1& swapchain_desc);
		void exit();

		// Used to allocate frame dynamic resources, such as constant buffers
		frame_alloc_t alloc_resource(uint64_t byte_size, uint64_t align = 0);
		void reset();

	}

}
