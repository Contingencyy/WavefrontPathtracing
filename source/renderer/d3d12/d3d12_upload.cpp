#include "pch.h"
#include "d3d12_upload.h"
#include "d3d12_common.h"
#include "d3d12_backend.h"
#include "d3d12_resource.h"

#include "core/thread.h"

namespace d3d12
{

	namespace upload
	{

		static u32 get_used_allocations()
		{
			u32 allocs_used = ((g_d3d->upload.alloc_head + UPLOAD_BUFFER_MAX_SUBMISSIONS) - g_d3d->upload.alloc_tail) % UPLOAD_BUFFER_MAX_SUBMISSIONS;
			return allocs_used;
		}

		static u32 get_free_allocations()
		{
			u32 allocs_used = ((g_d3d->upload.alloc_head + UPLOAD_BUFFER_MAX_SUBMISSIONS) - g_d3d->upload.alloc_tail) % UPLOAD_BUFFER_MAX_SUBMISSIONS;
			u32 allocs_free = UPLOAD_BUFFER_MAX_SUBMISSIONS - allocs_used;
			return allocs_free;
		}

		static void retire_single_allocation()
		{
			u32 retire_alloc_index = g_d3d->upload.alloc_tail % ARRAY_SIZE(g_d3d->upload.allocations);
			upload_alloc_t& retired_alloc = g_d3d->upload.allocations[retire_alloc_index];

			if (g_d3d->upload.fence->GetCompletedValue() < retired_alloc.fence_value)
			{
				d3d12::wait_on_fence(g_d3d->upload.fence, retired_alloc.fence_value);
			}

			g_d3d->upload.alloc_tail += 1;
			ring_buffer::free(g_d3d->upload.ring_buffer, retired_alloc.ring_buffer_alloc);
		}

		static void retire_all_allocations()
		{
			u32 allocs_used = get_used_allocations();

			while (allocs_used > 0)
			{
				retire_single_allocation();
				allocs_used = get_used_allocations();
			}
		}

		void init(u64 capacity)
		{
			g_d3d->upload.buffer = d3d12::create_buffer_upload(L"Ring Buffer Upload", capacity);
			g_d3d->upload.buffer_ptr = d3d12::map_resource(g_d3d->upload.buffer);

			g_d3d->upload.command_queue_copy = d3d12::create_command_queue(
				L"Ring Buffer Command Queue", D3D12_COMMAND_LIST_TYPE_COPY);
			g_d3d->upload.fence = d3d12::create_fence(L"Ring Buffer Fence");
			g_d3d->upload.fence_value = 0ull;

			for (u32 i = 0; i < UPLOAD_BUFFER_MAX_SUBMISSIONS; ++i)
			{
				upload_alloc_t& alloc = g_d3d->upload.allocations[i];

				alloc.d3d_resource = g_d3d->upload.buffer;
				alloc.d3d_command_allocator = d3d12::create_command_allocator(
					L"Ring Buffer Command Allocator", D3D12_COMMAND_LIST_TYPE_COPY);
				alloc.d3d_command_list = d3d12::create_command_list(L"Ring Buffer Command List",
					alloc.d3d_command_allocator, D3D12_COMMAND_LIST_TYPE_COPY);
				alloc.fence_value = 0ull;

				alloc.ring_buffer_alloc = {};
			}

			ring_buffer::init(g_d3d->upload.ring_buffer, capacity);
		}

		void exit()
		{
			ring_buffer::destroy(g_d3d->upload.ring_buffer);

			// Need to wait for the ring buffer transfer queue to be idle first
			d3d12::wait_on_fence(g_d3d->upload.fence, g_d3d->upload.fence_value);

			d3d12::unmap_resource(g_d3d->upload.buffer);
			d3d12::release_object(g_d3d->upload.buffer);

			d3d12::release_object(g_d3d->upload.command_queue_copy);
			d3d12::release_object(g_d3d->upload.fence);

			for (u32 i = 0; i < UPLOAD_BUFFER_MAX_SUBMISSIONS; ++i)
			{
				upload_alloc_t& alloc = g_d3d->upload.allocations[i];

				alloc.d3d_resource = nullptr;
				d3d12::release_object(alloc.d3d_command_allocator);
				d3d12::release_object(alloc.d3d_command_list);
				alloc.fence_value = 0ull;

				alloc.ring_buffer_alloc = {};
			}
		}

		upload_alloc_t& begin(u64 byte_count, u64 align)
		{
			upload_alloc_t* alloc = nullptr;

			thread::mutex::lock(g_d3d->upload.mutex);

			while (!alloc)
			{
				u32 allocs_free = get_free_allocations();

				if (allocs_free > 0)
				{
					ring_buffer_alloc_t ring_buffer_alloc = {};
					if (ring_buffer::alloc(g_d3d->upload.ring_buffer, byte_count, align, ring_buffer_alloc))
					{
						u32 alloc_index = g_d3d->upload.alloc_head % UPLOAD_BUFFER_MAX_SUBMISSIONS;

						alloc = &g_d3d->upload.allocations[alloc_index];
						alloc->ring_buffer_alloc = ring_buffer_alloc;
						alloc->ptr = PTR_OFFSET(g_d3d->upload.buffer_ptr, ring_buffer_alloc.byte_offset);

						if (alloc->fence_value > 0)
						{
							alloc->d3d_command_allocator->Reset();
							alloc->d3d_command_list->Reset(alloc->d3d_command_allocator, nullptr);
						}

						g_d3d->upload.alloc_head += 1;
					}
				}
				
				if (!alloc)
				{
					ASSERT(get_used_allocations() > 0);
					retire_single_allocation();
				}
			}

			thread::mutex::unlock(g_d3d->upload.mutex);

			ASSERT(alloc);
			return *alloc;
		}

		u64 end(upload_alloc_t& alloc)
		{
			u64 fence_value = 0;

			thread::mutex::lock(g_d3d->upload.mutex);

			fence_value = ++g_d3d->upload.fence_value;
			alloc.fence_value = fence_value;

			alloc.d3d_command_list->Close();
			ID3D12CommandList* d3d_command_lists[] = { alloc.d3d_command_list };
			g_d3d->upload.command_queue_copy->ExecuteCommandLists(1, d3d_command_lists);
			g_d3d->upload.command_queue_copy->Signal(g_d3d->upload.fence, alloc.fence_value);

			thread::mutex::unlock(g_d3d->upload.mutex);

			return fence_value;
		}

		void flush()
		{
			d3d12::wait_on_fence(g_d3d->upload.fence, g_d3d->upload.fence_value);
			retire_all_allocations();
		}

	}

}
