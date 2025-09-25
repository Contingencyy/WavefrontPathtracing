#pragma once
#include "d3d12_include.h"
#include "d3d12_descriptor.h"

#include "core/common.h"
#include "core/allocators/linear_alloc.h"

namespace d3d12
{

	inline constexpr uint64_t FRAME_ALLOCATOR_DEFAULT_CAPACITY = MB(16);

	struct frame_context_t
	{
		ID3D12CommandAllocator* command_allocator;
		ID3D12GraphicsCommandList10* command_list;

		ID3D12Resource* backbuffer;
		descriptor_allocation_t backbuffer_rtv;
		uint64_t fence_value;

		ID3D12Resource* frame_allocator_resource;
		linear_alloc_t frame_linear_allocator;
		uint8_t* frame_allocator_ptr;

		ID3D12Resource* readback_buffer_timestamps;
	};

	struct frame_resource_t
	{
		ID3D12Resource* resource;
		uint64_t byte_offset;
		uint64_t byte_count;

		uint8_t* ptr;
	};

	void init_frame_contexts(uint64_t frame_alloc_capacity, const DXGI_SWAP_CHAIN_DESC1& swapchain_desc);
	void destroy_frame_contexts();

	// Used to allocate frame dynamic resources, such as constant buffers
	frame_resource_t allocate_frame_resource(uint64_t byte_size, uint64_t align = 0);
	void reset_frame_context();

}
