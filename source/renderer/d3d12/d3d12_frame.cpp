#include "pch.h"
#include "d3d12_frame.h"
#include "d3d12_common.h"
#include "d3d12_resource.h"
#include "d3d12_backend.h"

namespace d3d12
{
	namespace frame
	{

		void init(uint64_t resource_capacity, const DXGI_SWAP_CHAIN_DESC1& swapchain_desc)
		{
			// Create render target views for all back buffers
			for (uint32_t i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
			{
				frame_context_t& frame_ctx = g_d3d->frame_ctx[i];

				// Create back buffer render target views
				g_d3d->swapchain.dxgi_swapchain->GetBuffer(i, IID_PPV_ARGS(&frame_ctx.backbuffer));
				ARENA_SCRATCH_SCOPE()
				{
					frame_ctx.backbuffer->SetName(ARENA_WPRINTF(arena_scratch, L"Back Buffer %u", i).buf);
					frame_ctx.backbuffer_rtv = descriptor::alloc(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
					create_texture_2d_rtv(frame_ctx.backbuffer, frame_ctx.backbuffer_rtv, 0, swapchain_desc.Format);

					// Create linear allocator and underlying resource
					frame_ctx.per_frame_resource = create_buffer_upload(ARENA_WPRINTF(arena_scratch, L"Frame allocator buffer %u", i).buf, resource_capacity);
				}
				frame_ctx.per_frame_ptr = (uint8_t*)map_resource(frame_ctx.per_frame_resource);
				linear_alloc::init(frame_ctx.per_frame_allocator, resource_capacity);
			}
		}

		void exit()
		{
			for (uint32_t i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
			{
				frame_context_t& frame_ctx = g_d3d->frame_ctx[i];

				// Destroy per-frame linear allocator and resource
				linear_alloc::destroy(frame_ctx.per_frame_allocator);
				unmap_resource(frame_ctx.per_frame_resource);
				DX_RELEASE_OBJECT(frame_ctx.per_frame_resource);
			}
		}

		frame_alloc_t alloc_resource(uint64_t byte_size, uint64_t align)
		{
			frame_context_t& frame_ctx = get_frame_context();

			frame_alloc_t alloc = {};
			bool result = linear_alloc::alloc(frame_ctx.per_frame_allocator, alloc.byte_offset, byte_size, align);
			ASSERT(result);

			alloc.resource = frame_ctx.per_frame_resource;
			alloc.byte_count = byte_size;
			alloc.ptr = PTR_OFFSET(frame_ctx.per_frame_ptr, alloc.byte_offset);

			return alloc;
		}

		void reset()
		{
			frame_context_t& frame_ctx = get_frame_context();

			// Reset frame context command allocator and command list
			if (frame_ctx.fence_value > 0)
			{
				DX_CHECK_HR(frame_ctx.command_allocator->Reset());
				DX_CHECK_HR(frame_ctx.command_list->Reset(frame_ctx.command_allocator, nullptr));
			}

			// Reset linear allocator for per-frame data
			linear_alloc::reset(frame_ctx.per_frame_allocator);
		}

	}
}
