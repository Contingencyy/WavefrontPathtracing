#pragma once
#include "dx12_include.h"

struct descriptor_allocation_t;

namespace dx12_backend
{

	// --------------------------------------------------------------------------------------------------------
	// ---------- Buffers

	ID3D12Resource* create_buffer(const wchar_t* name, u64 byte_size);
	ID3D12Resource* create_buffer_upload(const wchar_t* name, u64 byte_size);

	void create_buffer_cbv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u64 byte_count, u64 byte_offset = 0);
	// Used for creating a ByteAddressBuffer view
	void create_buffer_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u64 byte_count, u64 byte_offset = 0);
	// Used for creating a StructuredBuffer view
	void create_buffer_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u32 element_count, u32 element_stride, u32 element_offset = 0);
	// Used for creating a RWByteAddressBuffer view
	void create_buffer_uav(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u64 byte_count, u64 byte_offset);
	// Used for creating a RWStructuredBuffer view
	void create_buffer_uav(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u32 element_count, u32 element_stride, u32 element_offset = 0);

	// --------------------------------------------------------------------------------------------------------
	// ---------- Textures

	ID3D12Resource* create_texture_2d(const wchar_t* name, DXGI_FORMAT format, u32 width, u32 height, u32 mips,
		D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear_value, D3D12_RESOURCE_FLAGS flags);

	void create_texture_2d_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, DXGI_FORMAT format, u32 mip_count = 0xFFFFFFFF, u32 mip_bias = 0);
	void create_texture_2d_uav(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u32 mip = 0);
	void create_texture_2d_rtv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, DXGI_FORMAT format, u32 mip = 0);
	void create_texture_2d_dsv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, DXGI_FORMAT format, u32 mip = 0);

}
