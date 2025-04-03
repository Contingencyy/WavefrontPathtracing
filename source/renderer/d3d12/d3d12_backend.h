#pragma once
#include "d3d12_include.h"

namespace d3d12
{

	struct backend_init_params
	{
		memory_arena_t* arena;
		uint32_t back_buffer_count;
	};
	
	void init(const backend_init_params& init_params);
	void exit();

	void begin_frame();
	void end_frame();

	void copy_to_back_buffer(ID3D12Resource* src_resource, uint32_t render_width, uint32_t render_height);
	void render_imgui();
	void present();

	void flush();
	void release_object(ID3D12Object* object);

	ID3D12CommandQueue* create_command_queue(const wchar_t* debug_name, D3D12_COMMAND_LIST_TYPE type);
	ID3D12CommandAllocator* create_command_allocator(const wchar_t* debug_name, D3D12_COMMAND_LIST_TYPE type);
	ID3D12GraphicsCommandList10* create_command_list(const wchar_t* debug_name, ID3D12CommandAllocator* d3d_command_allocator, D3D12_COMMAND_LIST_TYPE type);

	ID3D12Fence* create_fence(const wchar_t* debug_name, uint64_t initial_fence_value = 0);
	void wait_on_fence(ID3D12Fence* fence, uint64_t wait_on_fence_value);
	
	IDxcBlob* compile_shader(const wchar_t* filepath, const wchar_t* entry_point, const wchar_t* target_profile,
		uint32_t define_count = 0, const DxcDefine* defines = nullptr);
	ID3D12RootSignature* create_root_signature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& root_signature_desc);
	ID3D12PipelineState* create_pso_cs(IDxcBlob* cs_shader_blob, ID3D12RootSignature* cs_root_sig);

	void* map_resource(ID3D12Resource* resource, uint32_t subresource = 0, uint64_t byte_offset = 0, uint64_t byte_count = D3D12_MAP_FULL_RANGE);
	void unmap_resource(ID3D12Resource* resource, uint32_t subresource = 0, uint64_t byte_offset = 0, uint64_t byte_count = D3D12_MAP_FULL_RANGE);

	// Will block if the command list has not finished yet
	// TODO: Implement command list pool for immediate tasks
	ID3D12GraphicsCommandList10* get_immediate_command_list();
	uint64_t execute_immediate_command_list(ID3D12GraphicsCommandList10* command_list, bool wait_on_finish);

	struct device_memory_info_t
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO local_mem;
		DXGI_QUERY_VIDEO_MEMORY_INFO non_local_mem;
	};
	device_memory_info_t get_device_memory_info();

}
