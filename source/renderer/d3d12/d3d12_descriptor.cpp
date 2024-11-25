#include "pch.h"
#include "d3d12_descriptor.h"
#include "d3d12_common.h"

namespace d3d12
{

	namespace descriptor
	{

		struct descriptor_block_t
		{
			u32 offset = 0;
			u32 count = 0;

			descriptor_block_t* next = nullptr;
		};

		struct instance_t
		{
			struct descriptor_blocks_t
			{
				descriptor_block_t* rtv = nullptr;
				descriptor_block_t* cbv_srv_uav = nullptr;

				descriptor_block_t* free_blocks = nullptr;
			} descriptor_blocks;
		} static *inst;

		static descriptor_block_t* get_descriptor_blocks_by_type(D3D12_DESCRIPTOR_HEAP_TYPE type)
		{
			switch (type)
			{
			case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 return inst->descriptor_blocks.rtv;
			case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return inst->descriptor_blocks.cbv_srv_uav;
			default:									 FATAL_ERROR("D3D12", "Tried to get descriptor block head for a descriptor heap type that is not supported or invalid");
			}

			return nullptr;
		}

		static void set_descriptor_block_head_for_type(D3D12_DESCRIPTOR_HEAP_TYPE type, descriptor_block_t* block)
		{
			switch (type)
			{
			case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 if (block) { block->next = inst->descriptor_blocks.rtv; } inst->descriptor_blocks.rtv = block; break;
			case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: if (block) { block->next = inst->descriptor_blocks.cbv_srv_uav; } inst->descriptor_blocks.cbv_srv_uav = block; break;
			default:									 FATAL_ERROR("D3D12", "Tried to set descriptor block head for a descriptor heap type that is not supported or invalid");
			}
		}

		static void set_descriptor_block_head_freelist(descriptor_block_t* block)
		{
			block->offset = 0;
			block->count = 0;

			block->next = inst->descriptor_blocks.free_blocks;
			inst->descriptor_blocks.free_blocks = block;
		}
		
		void init()
		{
			inst = ARENA_ALLOC_STRUCT_ZERO(g_d3d->arena, instance_t);

			// Create descriptor heaps
			D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
			descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			descriptor_heap_desc.NumDescriptors = g_d3d->descriptor_heaps.heap_sizes.rtv;
			descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			descriptor_heap_desc.NodeMask = 0;
			DX_CHECK_HR(g_d3d->device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&g_d3d->descriptor_heaps.rtv)));

			descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			descriptor_heap_desc.NumDescriptors = g_d3d->descriptor_heaps.heap_sizes.cbv_srv_uav;
			descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			DX_CHECK_HR(g_d3d->device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&g_d3d->descriptor_heaps.cbv_srv_uav)));

			// Get descriptor increment sizes
			g_d3d->descriptor_heaps.handle_sizes.rtv = g_d3d->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			g_d3d->descriptor_heaps.handle_sizes.cbv_srv_uav = g_d3d->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// Initialize descriptor blocks per descriptor heap
			inst->descriptor_blocks.rtv = ARENA_ALLOC_STRUCT_ZERO(g_d3d->arena, descriptor_block_t);
			inst->descriptor_blocks.rtv[0].offset = 0;
			inst->descriptor_blocks.rtv[0].count = get_descriptor_heap_size_by_type(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			inst->descriptor_blocks.cbv_srv_uav = ARENA_ALLOC_STRUCT_ZERO(g_d3d->arena, descriptor_block_t);
			inst->descriptor_blocks.cbv_srv_uav[0].offset = 0;
			inst->descriptor_blocks.cbv_srv_uav[0].count = get_descriptor_heap_size_by_type(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		void exit()
		{
		}

		descriptor_allocation_t alloc(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 count)
		{
			descriptor_block_t* descriptor_block = get_descriptor_blocks_by_type(type);
			descriptor_block_t* descriptor_block_prev = nullptr;

			// Traverse descriptor block linked-list until we find a block large enough
			while (descriptor_block)
			{
				if (descriptor_block->count >= count)
					break;

				descriptor_block_prev = descriptor_block;
				descriptor_block = descriptor_block->next;
			}

			// Could not find a descriptor block that can satisfy the allocation
			if (!descriptor_block)
				FATAL_ERROR("D3D12", "Could not find a descriptor block that can fit %u descriptors", count);

			descriptor_allocation_t alloc = {};

			// If we found a descriptor block sufficiently big, make the allocation from that block
			if (descriptor_block)
			{
				alloc.type = type;
				alloc.offset = descriptor_block->offset;
				alloc.count = count;
				alloc.cpu = get_descriptor_heap_cpu_handle(alloc.type, alloc.offset);
				alloc.gpu = get_descriptor_heap_gpu_handle(alloc.type, alloc.offset);
				alloc.increment_size = get_descriptor_handle_size_by_type(alloc.type);

				descriptor_block->offset += count;
				descriptor_block->count -= count;

				// If the descriptor block got shrunk to size 0, we need to take it out of the linked list
				if (descriptor_block->count == 0)
				{
					if (descriptor_block_prev)
						descriptor_block_prev->next = descriptor_block->next;
					else
						set_descriptor_block_head_for_type(type, descriptor_block->next);

					// Append block to free blocks list to repurpose
					set_descriptor_block_head_freelist(descriptor_block);
				}
			}

			return alloc;
		}

		void free(const descriptor_allocation_t& descriptor)
		{
			descriptor_block_t* descriptor_block = get_descriptor_blocks_by_type(descriptor.type);
			descriptor_block_t* descriptor_block_prev = nullptr;

			// If there is not a single block of free descriptors, create a new one
			if (!descriptor_block)
			{
				descriptor_block_t* descriptor_block_new = ARENA_ALLOC_STRUCT_ZERO(g_d3d->arena, descriptor_block_t);
				descriptor_block_new->offset = descriptor.offset;
				descriptor_block_new->count = descriptor.count;

				set_descriptor_block_head_for_type(descriptor.type, descriptor_block_new);
				return;
			}

			while (descriptor_block)
			{
				// If the current descriptor block is on the left of the descriptors to free, we can grow the free block
				if (descriptor_block->offset + descriptor_block->count == descriptor.offset)
				{
					descriptor_block->count += descriptor.count;

					// If the next block to the one we just grew is now directly adjacent on the right, we combine them too
					if (descriptor_block->next &&
						descriptor_block->next->offset == descriptor_block->offset + descriptor_block->count)
					{
						descriptor_block->count += descriptor_block->next->count;
						descriptor_block_t* next_block = descriptor_block->next;
						descriptor_block->next = next_block->next;

						set_descriptor_block_head_freelist(next_block);
					}

					return;
				}
				else if (descriptor_block->offset > descriptor.offset)
				{
					descriptor_block_t* descriptor_block_new = ARENA_ALLOC_STRUCT_ZERO(g_d3d->arena, descriptor_block_t);
					descriptor_block_new->offset = descriptor.offset;
					descriptor_block_new->count = descriptor.count;

					// Insert the new block into the chain, or to the head if there was no previous block
					if (descriptor_block_prev)
					{
						descriptor_block_prev->next = descriptor_block_new;
						descriptor_block_new->next = descriptor_block;
					}
					else
					{
						descriptor_block_new->next = descriptor_block;
						set_descriptor_block_head_for_type(descriptor.type, descriptor_block_new);
					}
					
					// Merge the new block and the block to its right if possible
					if (descriptor_block_new->next &&
						descriptor_block_new->offset + descriptor_block_new->count == descriptor_block_new->next->offset)
					{
						descriptor_block_new->count += descriptor_block_new->next->count;
						descriptor_block_t* descriptor_block_new_right = descriptor_block_new->next;
						descriptor_block_new->next = descriptor_block_new_right->next;

						set_descriptor_block_head_freelist(descriptor_block_new_right);
					}

					// Merge the new block and the previous block to its left if possible
					if (descriptor_block_prev &&
						descriptor_block_prev->offset + descriptor_block_prev->count == descriptor_block_new->offset)
					{
						descriptor_block_prev->count += descriptor_block_new->count;
						descriptor_block_prev->next = descriptor_block_new->next;

						set_descriptor_block_head_freelist(descriptor_block_new);
					}

					return;
				}
				else if (descriptor_block->offset == descriptor.offset)
				{
					// Somehow tried to free the same descriptors multiple times
					ASSERT(descriptor_block->offset != descriptor.offset);
				}

				descriptor_block_prev = descriptor_block;
				descriptor_block = descriptor_block->next;
			}

			// Somehow failed to free descriptors
			ASSERT(false);
		}

		bool is_valid(const descriptor_allocation_t descriptor)
		{
			return (
				descriptor.type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES &&
				descriptor.count > 0);
		}

	}

}
