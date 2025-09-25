#include "d3d12_frame.h"
#include "d3d12_common.h"
#include "d3d12_resource.h"
#include "d3d12_backend.h"

#include "core/memory/memory_arena.h"

namespace d3d12
{

	void init_frame_contexts(uint64_t frame_alloc_capacity, const DXGI_SWAP_CHAIN_DESC1& swapchain_desc)
	{
		// Create render target views for all back buffers
		g_d3d->frame_ctx = ARENA_ALLOC_ARRAY_ZERO(g_d3d->arena, frame_context_t, g_d3d->swapchain.back_buffer_count);

		ARENA_SCRATCH_SCOPE()
		{
			for (uint32_t i = 0; i < g_d3d->swapchain.back_buffer_count; ++i)
			{
				frame_context_t& frame_ctx = g_d3d->frame_ctx[i];

				// Create back buffer render target views
				g_d3d->swapchain.dxgi_swapchain->GetBuffer(i, IID_PPV_ARGS(&frame_ctx.backbuffer));
				frame_ctx.backbuffer->SetName(ARENA_WPRINTF(arena_scratch, L"Back Buffer %u", i).buf);
				frame_ctx.backbuffer_rtv = allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				create_texture_2d_rtv(frame_ctx.backbuffer, frame_ctx.backbuffer_rtv, 0, swapchain_desc.Format);

				// Create command allocator and command list
				frame_ctx.command_allocator = create_command_allocator(ARENA_WPRINTF(arena_scratch, L"Frame Command Allocator %u", i).buf, D3D12_COMMAND_LIST_TYPE_DIRECT);
				frame_ctx.command_list = create_command_list(ARENA_WPRINTF(arena_scratch, L"Frame Command List %u", i).buf, g_d3d->frame_ctx[i].command_allocator, D3D12_COMMAND_LIST_TYPE_DIRECT);

				// Create linear allocator and underlying resource
				frame_ctx.frame_allocator_resource = create_buffer_upload(ARENA_WPRINTF(arena_scratch, L"Frame allocator buffer %u", i).buf, frame_alloc_capacity);
				
				frame_ctx.frame_allocator_ptr = (uint8_t*)map_resource(frame_ctx.frame_allocator_resource);
				linear_alloc::init(frame_ctx.frame_linear_allocator, frame_alloc_capacity);
			}
		}
	}

	void destroy_frame_contexts()
	{
		for (uint32_t i = 0; i < g_d3d->swapchain.back_buffer_count; ++i)
		{
			frame_context_t& frame_ctx = g_d3d->frame_ctx[i];

			// Destroy per-frame linear allocator and resource
			linear_alloc::destroy(frame_ctx.frame_linear_allocator);
			unmap_resource(frame_ctx.frame_allocator_resource);
			release_object(frame_ctx.frame_allocator_resource);
			release_object(frame_ctx.command_allocator);
			release_object(frame_ctx.command_list);
		}
	}

	frame_resource_t allocate_frame_resource(uint64_t byte_size, uint64_t align)
	{
		frame_context_t& frame_ctx = get_frame_context();

		frame_resource_t alloc = {};
		bool result = linear_alloc::alloc(frame_ctx.frame_linear_allocator, alloc.byte_offset, byte_size, align);
		ASSERT(result);

		alloc.resource = frame_ctx.frame_allocator_resource;
		alloc.byte_count = byte_size;
		alloc.ptr = PTR_OFFSET(frame_ctx.frame_allocator_ptr, alloc.byte_offset);

		return alloc;
	}

	void reset_frame_context()
	{
		frame_context_t& frame_ctx = get_frame_context();

		// Reset frame context command allocator and command list
		if (frame_ctx.fence_value > 0)
		{
			DX_CHECK_HR(frame_ctx.command_allocator->Reset());
			DX_CHECK_HR(frame_ctx.command_list->Reset(frame_ctx.command_allocator, nullptr));
		}

		// Reset linear allocator for per-frame data
		linear_alloc::reset(frame_ctx.frame_linear_allocator);
	}

}
