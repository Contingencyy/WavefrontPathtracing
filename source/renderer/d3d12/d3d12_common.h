#pragma once
#include "d3d12_include.h"
#include "d3d12_descriptor.h"
#include "d3d12_upload.h"
#include "d3d12_frame.h"

struct memory_arena_t;

namespace d3d12
{

	// TODO: Make amount of swap chain back buffers part of renderer init
	inline constexpr u32 SWAP_CHAIN_BACK_BUFFER_COUNT = 2u;
	inline constexpr u32 UPLOAD_BUFFER_MAX_SUBMISSIONS = 32u;
	inline constexpr u64 UPLOAD_BUFFER_CAPACITY = MB(64);
	inline constexpr u64 FRAME_ALLOCATOR_CAPACITY = MB(16);

	// -----------------------------------------------------------------------------------------
	// ---------- Structs

	struct d3d12_instance_t
	{
		memory_arena_t* arena;

		IDXGIAdapter4* dxgi_adapter;
		ID3D12Device8* device;
		ID3D12CommandQueue* command_queue_direct;

		b8 vsync;

		struct swapchain_t
		{
			IDXGISwapChain4* dxgi_swapchain;
			u32 back_buffer_index;
			b8 supports_tearing;

			u32 output_width;
			u32 output_height;
		} swapchain;

		struct sync_t
		{
			ID3D12Fence* fence;
			u64 fence_value;
		} sync;

		struct descriptor_heaps_t
		{
			ID3D12DescriptorHeap* rtv;
			ID3D12DescriptorHeap* cbv_srv_uav;

			struct handle_size_t
			{
				u32 rtv;
				u32 cbv_srv_uav;
			} handle_sizes;

			struct heap_size_t
			{
				u32 rtv;
				u32 cbv_srv_uav;
			} heap_sizes;
		} descriptor_heaps;

		struct shader_compiler_t
		{
			IDxcCompiler3* dx_compiler;
			IDxcUtils* dxc_utils;
			IDxcIncludeHandler* dxc_include_handler;
		} shader_compiler;

		struct upload_t
		{
			ID3D12Resource* buffer;
			void* buffer_ptr;

			ID3D12CommandQueue* command_queue_copy;
			ID3D12Fence* fence;
			u64 fence_value;

			upload_alloc_t allocations[UPLOAD_BUFFER_MAX_SUBMISSIONS];
			u32 alloc_head;
			u32 alloc_tail;

			ring_buffer_t ring_buffer;
			mutex_t mutex;
		} upload;

		frame_context_t frame_ctx[SWAP_CHAIN_BACK_BUFFER_COUNT];
	};
	extern d3d12_instance_t* g_d3d;

	// -----------------------------------------------------------------------------------------
	// ---------- Functions

	inline frame_context_t& get_frame_context()
	{
		return g_d3d->frame_ctx[g_d3d->swapchain.back_buffer_index];
	}

	inline ID3D12DescriptorHeap* get_descriptor_heap_by_type(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		switch (type)
		{
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 return g_d3d->descriptor_heaps.rtv;
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return g_d3d->descriptor_heaps.cbv_srv_uav;
		default:									 FATAL_ERROR("D3D12", "Tried to get descriptor heap for a descriptor heap type that is not supported or invalid");
		}

		return nullptr;
	}

	inline u32 get_descriptor_handle_size_by_type(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		switch (type)
		{
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 return g_d3d->descriptor_heaps.handle_sizes.rtv;
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return g_d3d->descriptor_heaps.handle_sizes.cbv_srv_uav;
		default:									 FATAL_ERROR("D3D12", "Tried to get descriptor handle size for a descriptor heap type that is not supported or invalid");
		}

		return 0;
	}

	inline u32 get_descriptor_heap_size_by_type(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		switch (type)
		{
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 return g_d3d->descriptor_heaps.heap_sizes.rtv;
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return g_d3d->descriptor_heaps.heap_sizes.cbv_srv_uav;
		default:									 FATAL_ERROR("D3D12", "Tried to get descriptor heap size for a descriptor heap type that is not supported or invalid");
		}

		return 0;
	}

	inline D3D12_CPU_DESCRIPTOR_HANDLE get_descriptor_heap_cpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 offset)
	{
		return D3D12_CPU_DESCRIPTOR_HANDLE(get_descriptor_heap_by_type(type)->GetCPUDescriptorHandleForHeapStart().ptr + offset * get_descriptor_handle_size_by_type(type));
	}

	inline D3D12_GPU_DESCRIPTOR_HANDLE get_descriptor_heap_gpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 offset)
	{
		// Only CBV_SRV_UAV and SAMPLER descriptor heaps can be shader visible (accessed through descriptor tables)
		if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		{
			return D3D12_GPU_DESCRIPTOR_HANDLE(get_descriptor_heap_by_type(type)->GetGPUDescriptorHandleForHeapStart().ptr + offset * get_descriptor_handle_size_by_type(type));
		}

		return D3D12_GPU_DESCRIPTOR_HANDLE(0);
	}

}
