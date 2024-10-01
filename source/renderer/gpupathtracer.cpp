#include "pch.h"
#include "gpupathtracer.h"
#include "renderer/renderer_common.h"

#include "dx12/dx12_common.h"
#include "dx12/dx12_backend.h"
#include "dx12/dx12_descriptor.h"
#include "dx12/dx12_resource.h"

#include "core/logger.h"

#include "imgui/imgui.h"

namespace dx12_backend
{

	static ID3D12RootSignature* create_root_signature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& root_signature_desc)
	{
		ID3DBlob* root_signature_serialized = nullptr;
		ID3DBlob* error_blob = nullptr;

		DX_CHECK_HR(D3D12SerializeVersionedRootSignature(&root_signature_desc, &root_signature_serialized, &error_blob));
		if (error_blob)
		{
			FATAL_ERROR("DX12 Backend", reinterpret_cast<char*>(error_blob->GetBufferPointer()));
		}
		DX_RELEASE_OBJECT(error_blob);

		ID3D12RootSignature* root_signature = nullptr;
		DX_CHECK_HR(g_dx12->d3d12_device->CreateRootSignature(0, root_signature_serialized->GetBufferPointer(),
			root_signature_serialized->GetBufferSize(), IID_PPV_ARGS(&root_signature)));

		DX_RELEASE_OBJECT(root_signature_serialized);
		return root_signature;
	}

	static IDxcBlob* compile_shader(const wchar_t* filepath, const wchar_t* entry_point, const wchar_t* target_profile)
	{
		HRESULT hr = 0;
		u32 codepage = 0;

		IDxcBlobEncoding* shader_source_blob = nullptr;
		DX_CHECK_HR(g_dx12->shader_compiler.dxc_utils->LoadFile(filepath, &codepage, &shader_source_blob));

		DxcBuffer shader_source_buffer = {};
		shader_source_buffer.Encoding = DXC_CP_ACP;
		shader_source_buffer.Ptr = shader_source_blob->GetBufferPointer();
		shader_source_buffer.Size = shader_source_blob->GetBufferSize();

		// TODO: Use string builder, add dynamic macros
		const wchar_t* compile_args[] =
		{
			filepath, // Filepath
			L"-E", entry_point, // Entry point
			L"-T", target_profile, // Target profile
			//L"-D <macro>", Defines a macro, macros with values can be defined like this: -D MacroName=MacroValue
			//L"-enable-16bit-types"
			DXC_ARG_WARNINGS_ARE_ERRORS,
			DXC_ARG_OPTIMIZATION_LEVEL3,
			DXC_ARG_PACK_MATRIX_ROW_MAJOR,
#ifdef _DEBUG
			DXC_ARG_DEBUG,
			DXC_ARG_SKIP_OPTIMIZATIONS
#endif
		};

		IDxcResult* compile_result = nullptr;
		DX_CHECK_HR(g_dx12->shader_compiler.dx_compiler->Compile(
			&shader_source_buffer, compile_args, ARRAY_SIZE(compile_args),
			g_dx12->shader_compiler.dxc_include_handler, IID_PPV_ARGS(&compile_result))
		);
		DX_CHECK_HR(compile_result->GetStatus(&hr));

		if (FAILED(hr))
		{
			IDxcBlobEncoding* compile_error = nullptr;
			compile_result->GetErrorBuffer(&compile_error);

			FATAL_ERROR("DX12 Backend", reinterpret_cast<char*>(compile_error->GetBufferPointer()));
			
			DX_RELEASE_OBJECT(compile_error);
			return nullptr;
		}

		IDxcBlob* shader_binary = nullptr;
		IDxcBlobUtf16* shader_name = nullptr;
		DX_CHECK_HR(compile_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_binary), &shader_name));

		if (!shader_binary)
		{
			FATAL_ERROR("DX12 Backend", "Failed to retrieve the shader binary");
		}

		DX_RELEASE_OBJECT(shader_source_blob);
		DX_RELEASE_OBJECT(shader_name);
		DX_RELEASE_OBJECT(compile_result);

		return shader_binary;
	}

	static ID3D12PipelineState* create_pipeline_state_compute(ID3D12RootSignature* root_signature, const wchar_t* filepath_cs)
	{
		IDxcBlob* cs_binary = compile_shader(filepath_cs, L"main", L"cs_6_7");

		D3D12_COMPUTE_PIPELINE_STATE_DESC cs_pipeline_desc = {};
		cs_pipeline_desc.pRootSignature = root_signature;
		cs_pipeline_desc.CS.pShaderBytecode = cs_binary->GetBufferPointer();
		cs_pipeline_desc.CS.BytecodeLength = cs_binary->GetBufferSize();
		cs_pipeline_desc.NodeMask = 0;
		cs_pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ID3D12PipelineState* cs_pipeline_state = nullptr;
		DX_CHECK_HR(g_dx12->d3d12_device->CreateComputePipelineState(&cs_pipeline_desc,	IID_PPV_ARGS(&cs_pipeline_state)));

		DX_RELEASE_OBJECT(cs_binary);

		return cs_pipeline_state;
	}

}

using namespace renderer;

namespace gpupathtracer
{

	struct instance_t
	{
		memory_arena_t* arena = nullptr;

		ID3D12PipelineState* pso = nullptr;
		ID3D12RootSignature* root_signature = nullptr;

		ID3D12Resource* render_target = nullptr;
		dx12_backend::descriptor_allocation_t render_target_uav = {};
		u32 render_target_uav_offset = 0;

		ID3D12Resource* tlas_gpu_resource = nullptr;
		dx12_backend::descriptor_allocation_t tlas_srv = {};
	} static *inst;

	void init(memory_arena_t* arena)
	{
		LOG_INFO("GPU Pathtracer", "Init");

		inst = ARENA_ALLOC_STRUCT_ZERO(arena, instance_t);
		inst->arena = arena;

		D3D12_DESCRIPTOR_RANGE1 descriptor_ranges[2] = {};
		descriptor_ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptor_ranges[0].NumDescriptors = dx12_backend::g_dx12->descriptor_heaps.heap_sizes.cbv_srv_uav;
		descriptor_ranges[0].BaseShaderRegister = 0;
		descriptor_ranges[0].RegisterSpace = 0;
		descriptor_ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;// D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		descriptor_ranges[0].OffsetInDescriptorsFromTableStart = 0;

		descriptor_ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		descriptor_ranges[1].NumDescriptors = dx12_backend::g_dx12->descriptor_heaps.heap_sizes.cbv_srv_uav;
		descriptor_ranges[1].BaseShaderRegister = 0;
		descriptor_ranges[1].RegisterSpace = 0;
		descriptor_ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;// D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		descriptor_ranges[1].OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER1 root_parameters[2] = {};
		root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_parameters[0].Descriptor.ShaderRegister = 0;
		root_parameters[0].Descriptor.RegisterSpace = 0;
		root_parameters[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		root_parameters[1].DescriptorTable.NumDescriptorRanges = ARRAY_SIZE(descriptor_ranges);
		root_parameters[1].DescriptorTable.pDescriptorRanges = descriptor_ranges;
		root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {};
		root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_2;
		root_signature_desc.Desc_1_2.NumParameters = ARRAY_SIZE(root_parameters);
		root_signature_desc.Desc_1_2.pParameters = root_parameters;
		root_signature_desc.Desc_1_2.NumStaticSamplers = 0;
		root_signature_desc.Desc_1_2.pStaticSamplers = nullptr;
		root_signature_desc.Desc_1_2.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

		inst->root_signature = dx12_backend::create_root_signature(root_signature_desc);
		inst->pso = dx12_backend::create_pipeline_state_compute(inst->root_signature, L"shaders/gpupathtracer.hlsl");

		inst->render_target = dx12_backend::create_texture_2d(L"Final Color Texture", DXGI_FORMAT_R8G8B8A8_UNORM,
			g_renderer->render_width, g_renderer->render_height, 1,
			D3D12_RESOURCE_STATE_COMMON, nullptr, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		inst->render_target_uav = dx12_backend::descriptor::alloc(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		dx12_backend::create_texture_2d_uav(inst->render_target, inst->render_target_uav, 0);
	}

	void exit()
	{
		LOG_INFO("GPU Pathtracer", "Exit");
	}

	void begin_frame()
	{
	}

	void end_frame()
	{
		dx12_backend::copy_to_back_buffer(inst->render_target, g_renderer->render_width, g_renderer->render_height);
	}

	void begin_scene(const camera_t& scene_camera)
	{
	}

	void render()
	{
		dx12_backend::frame_context_t& frame_ctx = dx12_backend::get_frame_context();

		// Upload the scene TLAS to the GPU


		// Set descriptor heap, needs to happen before clearing UAVs
		ID3D12DescriptorHeap* descriptor_heaps[1] = { dx12_backend::g_dx12->descriptor_heaps.cbv_srv_uav };
		frame_ctx.d3d12_command_list->SetDescriptorHeaps(ARRAY_SIZE(descriptor_heaps), descriptor_heaps);

		// Clear render target
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			barrier.Transition.pResource = inst->render_target;
			barrier.Transition.Subresource = 0;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

			frame_ctx.d3d12_command_list->ResourceBarrier(1, &barrier);
		}

#if 0
		float clear_value[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		// TODO: For clearing a UAV we need a handle to a shader visible heap and a non-shader visible heap.
		// I think it might be a good idea to simply have a scratch non-shader visible heap, and whenever we need to clear a UAV,
		// we simply allocate from that scratch heap and make a UAV on the spot before clearing, or only allocate one for textures used as render targets.
		frame_ctx.d3d12_command_list->ClearUnorderedAccessViewFloat(inst->render_target_uav.gpu,
			inst->render_target_uav.cpu, inst->render_target, clear_value, 0, nullptr);
#endif

		// Dispatch
		// TODO: Define these in a shared hlsl/cpp header, or set these as a define when compiling the shaders/PSO
		const u32 dispatch_threads_per_block_x = 16;
		const u32 dispatch_threads_per_block_y = 16;

		u32 dispatch_blocks_x = MAX((g_renderer->render_width + dispatch_threads_per_block_x - 1) / dispatch_threads_per_block_x, 1);
		u32 dispatch_blocks_y = MAX((g_renderer->render_height + dispatch_threads_per_block_y - 1) / dispatch_threads_per_block_y, 1);

		frame_ctx.d3d12_command_list->SetComputeRootSignature(inst->root_signature);
		frame_ctx.d3d12_command_list->SetPipelineState(inst->pso);
		frame_ctx.d3d12_command_list->Dispatch(dispatch_blocks_x, dispatch_blocks_y, 1);
	}

	void end_scene()
	{
	}

	void render_ui(b8 reset_accumulation)
	{
		if (ImGui::CollapsingHeader("GPU Pathtracer"))
		{
			ImGui::Text("PLACEHOLDER");
		}
	}

}
