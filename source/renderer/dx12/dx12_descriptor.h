#pragma once
#include "dx12_common.h"

namespace dx12_backend
{

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

	struct descriptor_allocation_t
	{
		D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		u32 offset = 0;
		u32 count = 0;
		u32 increment_size = 0;

		D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
	};

	namespace descriptor
	{

		void init();
		void exit();

		descriptor_allocation_t alloc(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 count);
		void free(const descriptor_allocation_t& descriptor);
		bool is_valid(const descriptor_allocation_t descriptor);

		inline D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(const descriptor_allocation_t& descriptor, u32 offset)
		{
			return D3D12_CPU_DESCRIPTOR_HANDLE(descriptor.cpu.ptr + offset * descriptor.increment_size);
		}

		inline D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(const descriptor_allocation_t& descriptor, u32 offset)
		{
			// Only CBV_SRV_UAV and SAMPLER descriptor heaps can be shader visible (accessed through descriptor tables)
			if (descriptor.type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || descriptor.type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
			{
				return D3D12_GPU_DESCRIPTOR_HANDLE(descriptor.gpu.ptr + offset * descriptor.increment_size);
			}

			return D3D12_GPU_DESCRIPTOR_HANDLE(0);
		}

	}

}
