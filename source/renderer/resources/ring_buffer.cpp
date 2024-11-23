#include "pch.h"
#include "ring_buffer.h"
#include "renderer/d3d12/d3d12_backend.h"
#include "renderer/d3d12/d3d12_resource.h"
#include "core/thread.h"

namespace ring_buffer
{

	static u32 get_used_allocations(ring_buffer_t& ring_buffer)
	{
		u32 allocs_total = ARRAY_SIZE(ring_buffer.allocations);
		u32 allocs_used = ((ring_buffer.alloc_head + allocs_total) - ring_buffer.alloc_tail) % allocs_total;
		return allocs_used;
	}

	static u32 get_free_allocations(ring_buffer_t& ring_buffer)
	{
		u32 allocs_total = ARRAY_SIZE(ring_buffer.allocations);
		u32 allocs_used = ((ring_buffer.alloc_head + allocs_total) - ring_buffer.alloc_tail) % allocs_total;
		u32 allocs_free = allocs_total - allocs_used;
		return allocs_free;
	}

	static void retire_single_allocation(ring_buffer_t& ring_buffer)
	{
		u32 retire_alloc_index = ring_buffer.alloc_tail % ARRAY_SIZE(ring_buffer.allocations);
		upload_alloc_t& retired_alloc = ring_buffer.allocations[retire_alloc_index];

		if (ring_buffer.d3d_fence->GetCompletedValue() < retired_alloc.fence_value)
		{
			d3d12::wait_on_fence(ring_buffer.d3d_fence, retired_alloc.fence_value);
		}

		ring_buffer.alloc_tail = (ring_buffer.alloc_tail + 1) % ARRAY_SIZE(ring_buffer.allocations);
		ring_buffer.tail = retired_alloc.head;
	}

	static void retire_all_allocations(ring_buffer_t& ring_buffer)
	{
		u32 allocs_used = get_used_allocations(ring_buffer);

		while (allocs_used > 0)
		{
			retire_single_allocation(ring_buffer);
			allocs_used = get_used_allocations(ring_buffer);
		}
	}

	void init(ring_buffer_t& ring_buffer, const wchar_t* name, u64 capacity)
	{
		ring_buffer.d3d_resource = d3d12::create_buffer_upload(name, capacity);

		ring_buffer.d3d_command_queue = d3d12::create_command_queue(
			L"Ring Buffer Command Queue", D3D12_COMMAND_LIST_TYPE_COPY);
		ring_buffer.d3d_fence = d3d12::create_fence(L"Ring Buffer Fence");
		ring_buffer.fence_value_at = 0u;

		ring_buffer.capacity = capacity;
		ring_buffer.head = 0;
		ring_buffer.tail = 0;
		ring_buffer.ptr_base = static_cast<u8*>(d3d12::map_resource(ring_buffer.d3d_resource));

		ring_buffer.alloc_head = 0;
		ring_buffer.alloc_tail = 0;

		for (u32 i = 0; i < RING_BUFFER_MAX_ALLOCATIONS; ++i)
		{
			upload_alloc_t& alloc = ring_buffer.allocations[i];

			alloc.d3d_resource = ring_buffer.d3d_resource;
			alloc.d3d_command_allocator = d3d12::create_command_allocator(
				L"Ring Buffer Command Allocator", D3D12_COMMAND_LIST_TYPE_COPY);
			alloc.d3d_command_list = d3d12::create_command_list(L"Ring Buffer Command List",
				alloc.d3d_command_allocator, D3D12_COMMAND_LIST_TYPE_COPY);

			alloc.fence_value = 0;
			alloc.head = 0;

			alloc.byte_offset = 0;
			alloc.byte_size = 0;
			alloc.ptr = nullptr;
		}
	}

	void destroy(ring_buffer_t& ring_buffer)
	{
		// Need to wait for the ring buffer transfer queue to be idle first
		d3d12::wait_on_fence(ring_buffer.d3d_fence, ring_buffer.fence_value_at);

		d3d12::unmap_resource(ring_buffer.d3d_resource);
		d3d12::release_object(ring_buffer.d3d_resource);

		d3d12::release_object(ring_buffer.d3d_command_queue);
		d3d12::release_object(ring_buffer.d3d_fence);

		for (u32 i = 0; i < RING_BUFFER_MAX_ALLOCATIONS; ++i)
		{
			upload_alloc_t& alloc = ring_buffer.allocations[i];

			d3d12::release_object(alloc.d3d_command_allocator);
			d3d12::release_object(alloc.d3d_command_list);
		}

		ring_buffer = {};
	}

	upload_alloc_t* alloc(ring_buffer_t& ring_buffer, u64 num_requested_bytes, u64 align)
	{
		ASSERT_MSG(align >= 4, "Ring buffer allocation should have a minimum alignment of 4 bytes");

		thread::mutex::lock(ring_buffer.mutex);
		upload_alloc_t* alloc = nullptr;

		// Set the maximum of requested bytes to the full ring buffer capacity.
		// Do this instead of handling a potential ring buffer resize.
		// The caller must handle allocations smaller than requested.
		num_requested_bytes = MIN(num_requested_bytes, ring_buffer.capacity);

		while (!alloc)
		{
			// Check if ring buffer has free allocations
			u32 allocs_free = get_free_allocations(ring_buffer);

			if (allocs_free > 0)
			{
				u64 aligned_head = ALIGN_UP_POW2(ring_buffer.head, align);

				u64 alloc_begin = aligned_head % ring_buffer.capacity;
				u64 alloc_end = (aligned_head + num_requested_bytes - 1) % ring_buffer.capacity;

				// If the allocation wraps to the beginning of the buffer, update the allocation to start at the beginning as well
				if (alloc_begin >= alloc_end)
				{
					aligned_head = 0;
					ring_buffer.head = 0;

					alloc_begin = aligned_head % ring_buffer.capacity;
					alloc_end = (aligned_head + num_requested_bytes - 1) % ring_buffer.capacity;
				}

				// If the tail is in front of the head we need to retire more [----H---------T------------]
				// allocations until the tail wraps around again as well      [T---H----------------------]
				// This effectively skips the end of the ring buffer whenever an allocation cannot fit perfectly into the end gap
				if (ring_buffer.tail <= aligned_head)
				{
					u64 bytes_used = aligned_head - ring_buffer.tail;
					u64 bytes_free = ring_buffer.capacity - bytes_used;

					// Check if we have enough bytes to fulfill the request
					if (bytes_free >= num_requested_bytes)
					{
						u64 alloc_begin = aligned_head % ring_buffer.capacity;
						u64 alloc_end = (aligned_head + num_requested_bytes - 1) % ring_buffer.capacity;

						// Sanity check whether the allocation request would wrap around to the beginning again
						if (alloc_begin < alloc_end)
						{
							alloc = &ring_buffer.allocations[ring_buffer.alloc_head];

							alloc->byte_offset = alloc_begin;
							alloc->byte_size = num_requested_bytes;
							alloc->head = aligned_head + alloc->byte_size;
							alloc->ptr = ring_buffer.ptr_base + alloc->byte_offset;

							ring_buffer.alloc_head = (ring_buffer.alloc_head + 1) % ARRAY_SIZE(ring_buffer.allocations);
							ring_buffer.head = alloc->head;

							// Reset command allocator and list before re-using the allocation
							if (alloc->fence_value > 0)
							{
								alloc->d3d_command_allocator->Reset();
								alloc->d3d_command_list->Reset(alloc->d3d_command_allocator, nullptr);
							}
						}
					}
				}
			}

			if (!alloc)
			{
				// Check if we can re-use another allocation, will block and wait to free a single one
				retire_single_allocation(ring_buffer);
			}
		}

		thread::mutex::unlock(ring_buffer.mutex);

		ASSERT(alloc);
		return alloc;
	}

	u64 submit(ring_buffer_t& ring_buffer, upload_alloc_t* upload_alloc)
	{
		u64 fence_value = 0;

		thread::mutex::lock(ring_buffer.mutex);

		fence_value = ++ring_buffer.fence_value_at;
		upload_alloc->fence_value = fence_value;

		upload_alloc->d3d_command_list->Close();
		ID3D12CommandList* d3d_command_lists[] = { upload_alloc->d3d_command_list };
		ring_buffer.d3d_command_queue->ExecuteCommandLists(1, d3d_command_lists);
		ring_buffer.d3d_command_queue->Signal(ring_buffer.d3d_fence, upload_alloc->fence_value);

		thread::mutex::unlock(ring_buffer.mutex);

		return fence_value;
	}

	void flush(ring_buffer_t& ring_buffer)
	{
		d3d12::wait_on_fence(ring_buffer.d3d_fence, ring_buffer.fence_value_at);
		retire_all_allocations(ring_buffer);
	}

}
