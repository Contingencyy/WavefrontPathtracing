#pragma once
#include "dx12_include.h"
#include "dx12_descriptor.h"

struct memory_arena_t;

namespace dx12_backend
{

	static constexpr u32 SWAP_CHAIN_BACK_BUFFER_COUNT = 2u;

	// -----------------------------------------------------------------------------------------
	// ---------- Structs

	struct frame_context_t
	{
		ID3D12CommandAllocator* d3d12_command_allocator = nullptr;
		ID3D12GraphicsCommandList6* d3d12_command_list = nullptr;

		ID3D12Resource* backbuffer = nullptr;
		descriptor_allocation_t backbuffer_rtv;
		u64 fence_value = 0;

		ID3D12Resource* cpu_pixel_upload_buffer = nullptr;
		u8* ptr_cpu_pixel_upload_buffer = nullptr;
	};

	struct dx12_instance_t
	{
		memory_arena_t* arena = nullptr;

		IDXGIAdapter4* dxgi_adapter = nullptr;
		ID3D12Device8* d3d12_device = nullptr;
		ID3D12CommandQueue* d3d12_command_queue_direct = nullptr;

		b8 vsync = false;

		frame_context_t frame_ctx[SWAP_CHAIN_BACK_BUFFER_COUNT];

		struct swapchain_t
		{
			IDXGISwapChain4* dxgi_swapchain = nullptr;
			u32 back_buffer_index = 0u;
			b8 supports_tearing = false;

			u32 output_width = 0;
			u32 output_height = 0;
		} swapchain;

		struct sync_t
		{
			ID3D12Fence* d3d12_fence = nullptr;
			u64 fence_value = 0ull;
		} sync;

		struct descriptor_heaps_t
		{
			ID3D12DescriptorHeap* rtv = nullptr;
			ID3D12DescriptorHeap* cbv_srv_uav = nullptr;

			struct handle_size_t
			{
				u32 rtv = 0;
				u32 cbv_srv_uav = 0;
			} handle_sizes;

			struct heap_size_t
			{
				u32 rtv = 0;
				u32 cbv_srv_uav = 0;
			} heap_sizes;
		} descriptor_heaps;

		struct shader_compiler_t
		{
			IDxcCompiler3* dx_compiler = nullptr;
			IDxcUtils* dxc_utils = nullptr;
			IDxcIncludeHandler* dxc_include_handler = nullptr;
		} shader_compiler;
	};
	extern dx12_instance_t* g_dx12;

	// -----------------------------------------------------------------------------------------
	// ---------- Functions

	inline frame_context_t& get_frame_context()
	{
		return g_dx12->frame_ctx[g_dx12->swapchain.back_buffer_index];
	}

	inline ID3D12DescriptorHeap* get_descriptor_heap_by_type(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		switch (type)
		{
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 return g_dx12->descriptor_heaps.rtv;
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return g_dx12->descriptor_heaps.cbv_srv_uav;
		default:									 FATAL_ERROR("DX12 Backend", "Tried to get descriptor heap for a descriptor heap type that is not supported or invalid");
		}

		return nullptr;
	}

	inline u32 get_descriptor_handle_size_by_type(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		switch (type)
		{
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 return g_dx12->descriptor_heaps.handle_sizes.rtv;
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return g_dx12->descriptor_heaps.handle_sizes.cbv_srv_uav;
		default:									 FATAL_ERROR("DX12 Backend", "Tried to get descriptor handle size for a descriptor heap type that is not supported or invalid");
		}

		return 0;
	}

	inline u32 get_descriptor_heap_size_by_type(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		switch (type)
		{
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:		 return g_dx12->descriptor_heaps.heap_sizes.rtv;
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return g_dx12->descriptor_heaps.heap_sizes.cbv_srv_uav;
		default:									 FATAL_ERROR("DX12 Backend", "Tried to get descriptor heap size for a descriptor heap type that is not supported or invalid");
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
