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
		u64 fence_value;

		ID3D12Resource* per_frame_resource;
		linear_alloc_t per_frame_allocator;
		u8* per_frame_ptr;
	};

	struct frame_alloc_t
	{
		ID3D12Resource* resource;
		u64 byte_offset;
		u64 byte_count;

		u8* ptr;
	};

	namespace frame
	{

		void init(u64 resource_capacity, const DXGI_SWAP_CHAIN_DESC1& swapchain_desc);
		void exit();

		frame_alloc_t alloc_resource(u64 byte_size, u64 align = 0);
		void reset();

	}
}
