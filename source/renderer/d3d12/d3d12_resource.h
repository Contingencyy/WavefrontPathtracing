#pragma once
#include "d3d12_include.h"

struct descriptor_allocation_t;

namespace d3d12
{

	// --------------------------------------------------------------------------------------------------------
	// ---------- Buffers

	ID3D12Resource* create_buffer(const wchar_t* name, uint64_t byte_size);
	ID3D12Resource* create_buffer_upload(const wchar_t* name, uint64_t byte_size);
	void create_buffer_blas(const wchar_t* name, ID3D12GraphicsCommandList10* command_list,
		ID3D12Resource* triangle_resource, uint64_t triangle_count, uint32_t triangle_stride,
		ID3D12Resource** out_blas_scratch, ID3D12Resource** out_blas);
	void create_buffer_tlas(const wchar_t* name, ID3D12GraphicsCommandList10* command_list,
		ID3D12Resource* instance_resource, uint32_t instance_count,
		ID3D12Resource** out_tlas_scratch, ID3D12Resource** out_tlas);

	void create_buffer_cbv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset, uint64_t byte_count, uint64_t byte_offset = 0);
	// Used for creating a ByteAddressBuffer view
	void create_buffer_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset, uint64_t byte_count, uint64_t byte_offset = 0);
	// Used for creating a StructuredBuffer view
	void create_buffer_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset, uint32_t element_count, uint32_t element_stride, uint32_t element_offset = 0);
	// Used for creating a RaytracingAccelerationStructure view
	void create_as_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset);
	// Used for creating a RWByteAddressBuffer view
	void create_buffer_uav(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset, uint64_t byte_count, uint64_t byte_offset = 0);
	// Used for creating a RWStructuredBuffer view
	void create_buffer_uav(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset, uint32_t element_count, uint32_t element_stride, uint32_t element_offset = 0);

	// --------------------------------------------------------------------------------------------------------
	// ---------- Textures

	ID3D12Resource* create_texture_2d(const wchar_t* name, DXGI_FORMAT format, uint32_t width, uint32_t height, uint32_t mips,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, const D3D12_CLEAR_VALUE* clear_value = nullptr);

	void create_texture_2d_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset, DXGI_FORMAT format, uint32_t mip_count = 0xFFFFFFFF, uint32_t mip_bias = 0);
	void create_texture_2d_uav(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset, uint32_t mip = 0);
	void create_texture_2d_rtv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset, DXGI_FORMAT format, uint32_t mip = 0);
	void create_texture_2d_dsv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, uint32_t descriptor_offset, DXGI_FORMAT format, uint32_t mip = 0);

	// --------------------------------------------------------------------------------------------------------
	// ---------- Barriers

	D3D12_RESOURCE_BARRIER barrier_transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after, uint32_t subresource = 0);
	D3D12_RESOURCE_BARRIER barrier_uav(ID3D12Resource* resource);
	D3D12_RESOURCE_BARRIER barrier_aliasing(ID3D12Resource* resource_before, ID3D12Resource* resource_after);

}
