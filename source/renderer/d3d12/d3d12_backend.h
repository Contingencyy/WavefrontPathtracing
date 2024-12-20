#pragma once
#include "d3d12_include.h"

namespace d3d12
{
	
	void init(memory_arena_t* arena);
	void exit();

	void begin_frame();
	void end_frame();

	void copy_to_back_buffer(ID3D12Resource* src_resource, u32 render_width, u32 render_height);
	void present();

	void release_object(ID3D12Object* object);

	ID3D12CommandQueue* create_command_queue(const wchar_t* debug_name, D3D12_COMMAND_LIST_TYPE type);
	ID3D12CommandAllocator* create_command_allocator(const wchar_t* debug_name, D3D12_COMMAND_LIST_TYPE type);
	ID3D12GraphicsCommandList6* create_command_list(const wchar_t* debug_name, ID3D12CommandAllocator* d3d_command_allocator, D3D12_COMMAND_LIST_TYPE type);

	ID3D12Fence* create_fence(const wchar_t* debug_name, u64 initial_fence_value = 0);
	void wait_on_fence(ID3D12Fence* fence, u64 wait_on_fence_value);
	
	IDxcBlob* compile_shader(const wchar_t* filepath, const wchar_t* entry_point, const wchar_t* target_profile);
	ID3D12RootSignature* create_root_signature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& root_signature_desc);
	ID3D12PipelineState* create_pso_cs(IDxcBlob* cs_shader_blob, ID3D12RootSignature* cs_root_sig);

	void* map_resource(ID3D12Resource* resource, u32 subresource = 0, u64 byte_offset = 0, u64 byte_count = D3D12_MAP_FULL_RANGE);
	void unmap_resource(ID3D12Resource* resource, u32 subresource = 0, u64 byte_offset = 0, u64 byte_count = D3D12_MAP_FULL_RANGE);

}
