#pragma once
#include "d3d12_include.h"

namespace d3d12
{

	struct descriptor_allocation_t
	{
		D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		uint32_t offset = 0;
		uint32_t count = 0;
		uint32_t increment_size = 0;

		D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
	};

	void init_descriptors();
	void exit_descriptors();

	descriptor_allocation_t allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count = 1);
	void free_descriptors(const descriptor_allocation_t& descriptor);
	bool is_valid_descriptor(const descriptor_allocation_t& descriptor);

	inline D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(const descriptor_allocation_t& descriptor, uint32_t offset)
	{
		return D3D12_CPU_DESCRIPTOR_HANDLE(descriptor.cpu.ptr + offset * descriptor.increment_size);
	}

	inline D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(const descriptor_allocation_t& descriptor, uint32_t offset)
	{
		// Only CBV_SRV_UAV and SAMPLER descriptor heaps can be shader visible (accessed through descriptor tables)
		if (descriptor.type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || descriptor.type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		{
			return D3D12_GPU_DESCRIPTOR_HANDLE(descriptor.gpu.ptr + offset * descriptor.increment_size);
		}

		return D3D12_GPU_DESCRIPTOR_HANDLE(0);
	}

}
