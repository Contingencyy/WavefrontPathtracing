#include "pch.h"
#include "d3d12_resource.h"
#include "d3d12_common.h"
#include "d3d12_descriptor.h"

namespace d3d12
{

	static ID3D12Resource* create_resource_internal(const wchar_t* name, const D3D12_HEAP_PROPERTIES& heap_props, const D3D12_RESOURCE_DESC& resource_desc)
	{
		ID3D12Resource* resource = nullptr;
		DX_CHECK_HR(g_d3d->device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE,
			&resource_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource)));

		// TODO: Add resource tracking

		resource->SetName(name);
		return resource;
	}

	ID3D12Resource* create_buffer(const wchar_t* name, u64 byte_size)
	{
		D3D12_HEAP_PROPERTIES heap_props = {};
		heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.Width = byte_size;
		resource_desc.Height = 1;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		return create_resource_internal(name, heap_props, resource_desc);
	}

	ID3D12Resource* create_buffer_upload(const wchar_t* name, u64 byte_size)
	{
		D3D12_HEAP_PROPERTIES heap_props = {};
		heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.Width = byte_size;
		resource_desc.Height = 1;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		return create_resource_internal(name, heap_props, resource_desc);
	}

	void create_buffer_cbv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u64 byte_count, u64 byte_offset)
	{
		ASSERT(resource);

#if D3D12_VALIDATE_RESOURCE_VIEWS
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
		u64 byte_range_end = byte_offset + byte_count;
		ASSERT(byte_range_end <= resource_desc.Width);
#endif

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
		cbv_desc.BufferLocation = resource->GetGPUVirtualAddress() + byte_offset;
		cbv_desc.SizeInBytes = byte_count;

		g_d3d->device->CreateConstantBufferView(&cbv_desc, descriptor::get_cpu_handle(descriptor, descriptor_offset));
	}

	void create_buffer_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u64 byte_count, u64 byte_offset)
	{
		ASSERT(resource);

#if D3D12_VALIDATE_RESOURCE_VIEWS
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
		u64 byte_range_end = byte_offset + byte_count;
		ASSERT(byte_range_end <= resource_desc.Width);
#endif

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		// If Buffer.StructureByteStride is 0, the format must be a valid format
		srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		srv_desc.Buffer.NumElements = byte_count / 4;
		srv_desc.Buffer.FirstElement = byte_offset;
		srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

		g_d3d->device->CreateShaderResourceView(resource, &srv_desc, descriptor::get_cpu_handle(descriptor, descriptor_offset));
	}

	void create_buffer_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u32 element_count, u32 element_stride, u32 element_offset)
	{
		ASSERT(resource);

#if D3D12_VALIDATE_RESOURCE_VIEWS
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
		u64 byte_range_end = element_offset * element_stride + element_count * element_stride;
		ASSERT(byte_range_end <= resource_desc.Width);
#endif

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		// If Buffer.StructureByteStride is 0, the format must be a valid format
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		srv_desc.Buffer.NumElements = element_count;
		srv_desc.Buffer.FirstElement = element_offset;
		srv_desc.Buffer.StructureByteStride = element_stride;
		srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		g_d3d->device->CreateShaderResourceView(resource, &srv_desc, descriptor::get_cpu_handle(descriptor, descriptor_offset));
	}

	void create_buffer_uav(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u64 byte_count, u64 byte_offset)
	{
		ASSERT(resource);

#if D3D12_VALIDATE_RESOURCE_VIEWS
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
		u64 byte_range_end = byte_offset + byte_count;
		ASSERT(byte_range_end <= resource_desc.Width);
#endif

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		// If Buffer.StructureByteStride is 0, the format must be a valid format
		uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		uav_desc.Buffer.NumElements = byte_count;
		uav_desc.Buffer.FirstElement = byte_offset;
		uav_desc.Buffer.StructureByteStride = 1;
		uav_desc.Buffer.CounterOffsetInBytes = 0;
		uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

		return g_d3d->device->CreateUnorderedAccessView(resource, nullptr, &uav_desc, descriptor::get_cpu_handle(descriptor, descriptor_offset));
	}

	void create_buffer_uav(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u32 element_count, u32 element_stride, u32 element_offset)
	{
		ASSERT(resource);

#if D3D12_VALIDATE_RESOURCE_VIEWS
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
		u64 byte_range_end = element_offset * element_stride + element_count * element_stride;
		ASSERT(byte_range_end <= resource_desc.Width);
#endif

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		// If Buffer.StructureByteStride is 0, the format must be a valid format
		uav_desc.Format = DXGI_FORMAT_UNKNOWN;
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		uav_desc.Buffer.NumElements = element_count;
		uav_desc.Buffer.FirstElement = element_offset;
		uav_desc.Buffer.StructureByteStride = element_stride;
		uav_desc.Buffer.CounterOffsetInBytes = 0;
		uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		return g_d3d->device->CreateUnorderedAccessView(resource, nullptr, &uav_desc, descriptor::get_cpu_handle(descriptor, descriptor_offset));
	}

	ID3D12Resource* create_texture_2d(const wchar_t* name, DXGI_FORMAT format, u32 width, u32 height, u32 mips, D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear_value, D3D12_RESOURCE_FLAGS flags)
	{
		D3D12_HEAP_PROPERTIES heap_props = {};
		heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resource_desc.Format = format;
		resource_desc.Width = (uint64_t)width;
		resource_desc.Height = height;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.Flags = flags;

		return create_resource_internal(name, heap_props, resource_desc);
	}

	void create_texture_2d_srv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, DXGI_FORMAT format, u32 mip_count, u32 mip_bias)
	{
		ASSERT(resource);
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();

#if D3D12_VALIDATE_RESOURCE_VIEWS
		ASSERT(mip_count + mip_bias <= resource_desc.MipLevels);
		ASSERT(!IS_BIT_FLAG_SET(resource_desc.Flags, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE));
#endif

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = format;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		srv_desc.Texture2D.MipLevels = mip_count == 0xFFFFFFFF ? resource_desc.MipLevels : mip_count;
		srv_desc.Texture2D.MostDetailedMip = mip_bias;
		srv_desc.Texture2D.PlaneSlice = 0;
		srv_desc.Texture2D.ResourceMinLODClamp = 0;

		g_d3d->device->CreateShaderResourceView(resource, &srv_desc, descriptor::get_cpu_handle(descriptor, descriptor_offset));
	}

	void create_texture_2d_uav(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, u32 mip)
	{
		ASSERT(resource);
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();

#if D3D12_VALIDATE_RESOURCE_VIEWS
		ASSERT(mip < resource_desc.MipLevels);
		ASSERT(IS_BIT_FLAG_SET(resource_desc.Flags, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
#endif

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format = resource_desc.Format;
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice = mip;
		uav_desc.Texture2D.PlaneSlice = 0;

		g_d3d->device->CreateUnorderedAccessView(resource, nullptr, &uav_desc, descriptor::get_cpu_handle(descriptor, descriptor_offset));
	}

	void create_texture_2d_rtv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, DXGI_FORMAT format, u32 mip)
	{
		ASSERT(resource);

#if D3D12_VALIDATE_RESOURCE_VIEWS
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
		ASSERT(mip < resource_desc.MipLevels);
		ASSERT(IS_BIT_FLAG_SET(resource_desc.Flags, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET));
#endif

		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.Format = format;
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = mip;
		rtv_desc.Texture2D.PlaneSlice = 0;

		g_d3d->device->CreateRenderTargetView(resource, &rtv_desc, descriptor::get_cpu_handle(descriptor, descriptor_offset));
	}

	void create_texture_2d_dsv(ID3D12Resource* resource, const descriptor_allocation_t& descriptor, u32 descriptor_offset, DXGI_FORMAT format, u32 mip)
	{
		ASSERT(resource);

#if D3D12_VALIDATE_RESOURCE_VIEWS
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
		ASSERT(mip < resource_desc.MipLevels);
		ASSERT(IS_BIT_FLAG_SET(resource_desc.Flags, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));
#endif

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
		dsv_desc.Format = format;
		dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv_desc.Texture2D.MipSlice = mip;
		// TODO: Determine only depth or only stencil based on format
		dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

		g_d3d->device->CreateDepthStencilView(resource, &dsv_desc, descriptor::get_cpu_handle(descriptor, descriptor_offset));
	}

}
