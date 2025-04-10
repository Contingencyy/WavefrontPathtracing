#pragma once
#include "d3d12_include.h"
#include "d3d12_descriptor.h"
#include "d3d12_upload.h"
#include "d3d12_frame.h"

struct memory_arena_t;

namespace d3d12
{

	// -----------------------------------------------------------------------------------------
	// ---------- Structs

	struct d3d12_instance_t
	{
		memory_arena_t arena;

		IDXGIAdapter4* dxgi_adapter;
		ID3D12Device14* device;
		ID3D12CommandQueue* command_queue_direct;

		bool vsync;

		struct swapchain_t
		{
			IDXGISwapChain4* dxgi_swapchain;
			uint32_t back_buffer_index;
			uint32_t back_buffer_count;
			bool supports_tearing;

			uint32_t output_width;
			uint32_t output_height;
		} swapchain;

		struct sync_t
		{
			ID3D12Fence* fence;
			uint64_t fence_value;
		} sync;

		struct descriptor_heaps_t
		{
			ID3D12DescriptorHeap* rtv;
			ID3D12DescriptorHeap* cbv_srv_uav;
			
			struct handle_size_t
			{
				uint32_t rtv;
				uint32_t cbv_srv_uav;
			} handle_sizes;

			struct heap_size_t
			{
				uint32_t rtv;
				uint32_t cbv_srv_uav;
			} heap_sizes;
		} descriptor_heaps;

		struct shader_compiler_t
		{
			IDxcCompiler3* dx_compiler;
			IDxcUtils* dxc_utils;
			IDxcIncludeHandler* dxc_include_handler;
		} shader_compiler;

		struct immediate_t
		{
			ID3D12CommandAllocator* command_allocator;
			ID3D12GraphicsCommandList10* command_list;
			ID3D12Fence* fence;
			uint64_t fence_value;
		} immediate;

		struct queries_t
		{
			uint32_t timestamp_query_capacity;
			uint32_t timestamp_query_at;
			uint32_t copy_queue_timestamp_query_at;
			
			ID3D12QueryHeap* timestamp_heap;
			ID3D12QueryHeap* copy_queue_timestamp_heap;
		} queries;

		upload_context_t upload_ctx;
		frame_context_t* frame_ctx;
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

	inline uint32_t get_descriptor_handle_size_by_type(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		switch (type)
		{
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 return g_d3d->descriptor_heaps.handle_sizes.rtv;
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return g_d3d->descriptor_heaps.handle_sizes.cbv_srv_uav;
		default:									 FATAL_ERROR("D3D12", "Tried to get descriptor handle size for a descriptor heap type that is not supported or invalid");
		}

		return 0;
	}

	inline uint32_t get_descriptor_heap_size_by_type(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		switch (type)
		{
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 return g_d3d->descriptor_heaps.heap_sizes.rtv;
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return g_d3d->descriptor_heaps.heap_sizes.cbv_srv_uav;
		default:									 FATAL_ERROR("D3D12", "Tried to get descriptor heap size for a descriptor heap type that is not supported or invalid");
		}

		return 0;
	}

	inline D3D12_CPU_DESCRIPTOR_HANDLE get_descriptor_heap_cpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t offset)
	{
		return D3D12_CPU_DESCRIPTOR_HANDLE(get_descriptor_heap_by_type(type)->GetCPUDescriptorHandleForHeapStart().ptr + offset * get_descriptor_handle_size_by_type(type));
	}

	inline D3D12_GPU_DESCRIPTOR_HANDLE get_descriptor_heap_gpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t offset)
	{
		// Only CBV_SRV_UAV and SAMPLER descriptor heaps can be shader visible (accessed through descriptor tables)
		if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		{
			return D3D12_GPU_DESCRIPTOR_HANDLE(get_descriptor_heap_by_type(type)->GetGPUDescriptorHandleForHeapStart().ptr + offset * get_descriptor_handle_size_by_type(type));
		}

		return D3D12_GPU_DESCRIPTOR_HANDLE(0);
	}

}
