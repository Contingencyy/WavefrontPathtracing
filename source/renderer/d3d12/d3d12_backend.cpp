#include "pch.h"
#include "d3d12_backend.h"
#include "d3d12_common.h"
#include "d3d12_resource.h"
#include "d3d12_descriptor.h"
#include "d3d12_upload.h"
#include "d3d12_frame.h"

#include "core/logger.h"
#include "core/memory/memory_arena.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

// Specify the D3D12 agility SDK version and path
// This will help the D3D12.dll loader pick the right D3D12Core.dll (either the system g_d3dalled or provided agility)
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 715; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace d3d12
{

	d3d12_instance_t* g_d3d = nullptr;

	static D3D12_RESOURCE_BARRIER transition_barrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = state_before;
		barrier.Transition.StateAfter = state_after;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		return barrier;
	}

	void init(memory_arena_t* arena)
	{
		LOG_INFO("D3D12", "Init");

		g_d3d = ARENA_ALLOC_STRUCT_ZERO(arena, d3d12_instance_t);
		g_d3d->arena = arena;

		// Enable debug layer
#ifdef _DEBUG
		ID3D12Debug* debug_interface = nullptr;
		DX_CHECK_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
		debug_interface->EnableDebugLayer();

		// Enable GPU based validation
#ifdef DX12_GPU_BASED_VALIDATION
		ID3D12Debug1* debug_interface1 = nullptr;
		DXCheckHR(debug_interface->QueryInterface(IID_PPV_ARGS(&debug_interface1)));
		debug_interface1->SetEnableGPUBasedValidation(true);
#endif
#endif

		u32 factory_create_flags = 0;
#ifdef _DEBUG
		factory_create_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		// Create factory
		IDXGIFactory7* dxgi_factory7 = nullptr;
		DX_CHECK_HR(CreateDXGIFactory2(factory_create_flags, IID_PPV_ARGS(&dxgi_factory7)));

		// Select adapter
		D3D_FEATURE_LEVEL d3d_min_feature_level = D3D_FEATURE_LEVEL_12_2;

		IDXGIAdapter1* dxgi_adapter1 = nullptr;
		u64 max_dedicated_video_mem = 0;

		for (u32 adapter_idx = 0; dxgi_factory7->EnumAdapters1(adapter_idx, &dxgi_adapter1) != DXGI_ERROR_NOT_FOUND; ++adapter_idx)
		{
			DXGI_ADAPTER_DESC1 dxgi_adapter_desc = {};
			DX_CHECK_HR(dxgi_adapter1->GetDesc1(&dxgi_adapter_desc));

			if ((dxgi_adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgi_adapter1, d3d_min_feature_level, __uuidof(ID3D12Device), nullptr)) &&
				dxgi_adapter_desc.DedicatedVideoMemory > max_dedicated_video_mem)
			{
				max_dedicated_video_mem = dxgi_adapter_desc.DedicatedVideoMemory;
				DX_CHECK_HR(dxgi_adapter1->QueryInterface(IID_PPV_ARGS(&g_d3d->dxgi_adapter)));
			}
		}

		// Create D3D12 device
		DX_CHECK_HR(D3D12CreateDevice(g_d3d->dxgi_adapter, d3d_min_feature_level, IID_PPV_ARGS(&g_d3d->device)));

		// Check if additional required features are supported
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 d3d_options12 = {};
		DX_CHECK_HR(g_d3d->device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &d3d_options12, sizeof(d3d_options12)));

		if (!d3d_options12.EnhancedBarriersSupported)
			FATAL_ERROR("D3D12", "DirectX 12 Feature not supported: Enhanced Barriers");

#ifdef _DEBUG
		// Set info queue behavior and filters
		ID3D12InfoQueue* d3d12_info_queue = nullptr;
		DX_CHECK_HR(g_d3d->device->QueryInterface(IID_PPV_ARGS(&d3d12_info_queue)));
		DX_CHECK_HR(d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
		DX_CHECK_HR(d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
		DX_CHECK_HR(d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
#endif

		// Check for tearing support
		BOOL allow_tearing = FALSE;
		DX_CHECK_HR(dxgi_factory7->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(BOOL)));
		g_d3d->swapchain.supports_tearing = (allow_tearing == TRUE);

		// Create swap chain command queue
		g_d3d->command_queue_direct = create_command_queue(L"Present Command Queue Direct", D3D12_COMMAND_LIST_TYPE_DIRECT);

		// Create swap chain
		HWND hwnd = GetActiveWindow();

		RECT window_rect = {};
		GetClientRect(hwnd, &window_rect);

		g_d3d->swapchain.output_width = window_rect.right - window_rect.left;
		g_d3d->swapchain.output_height = window_rect.bottom - window_rect.top;

		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
		swapchain_desc.Width = g_d3d->swapchain.output_width;
		swapchain_desc.Height = g_d3d->swapchain.output_height;
		swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_desc.Stereo = FALSE;
		swapchain_desc.SampleDesc = { 1, 0 };
		swapchain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		swapchain_desc.BufferCount = SWAP_CHAIN_BACK_BUFFER_COUNT;
		swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
		swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapchain_desc.Flags = g_d3d->swapchain.supports_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		IDXGISwapChain1* dxgi_swapchain1 = nullptr;
		DX_CHECK_HR(dxgi_factory7->CreateSwapChainForHwnd(g_d3d->command_queue_direct, hwnd, &swapchain_desc, nullptr, nullptr, &dxgi_swapchain1));
		DX_CHECK_HR(dxgi_swapchain1->QueryInterface(&g_d3d->swapchain.dxgi_swapchain));
		DX_CHECK_HR(dxgi_factory7->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		g_d3d->swapchain.back_buffer_index = g_d3d->swapchain.dxgi_swapchain->GetCurrentBackBufferIndex();

		// Create command allocators and command lists
		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			g_d3d->frame_ctx[i].command_allocator = create_command_allocator(L"Frame Command Allocator", D3D12_COMMAND_LIST_TYPE_DIRECT);
			g_d3d->frame_ctx[i].command_list = create_command_list(
				L"Frame Command List", g_d3d->frame_ctx[i].command_allocator, D3D12_COMMAND_LIST_TYPE_DIRECT);
		}

		// Create fence and fence event
		g_d3d->sync.fence = create_fence(L"Frame Fence");

		// Initialize descriptor heaps, descriptor allocation
		g_d3d->descriptor_heaps.heap_sizes.rtv = SWAP_CHAIN_BACK_BUFFER_COUNT;
		g_d3d->descriptor_heaps.heap_sizes.cbv_srv_uav = 1024;
		descriptor::init();

		// Initialize upload ring buffer
		upload::init(UPLOAD_CAPACITY);

		// Initialize per-frame contexts
		frame::init(PER_FRAME_RESOURCE_CAPACITY, swapchain_desc);

		// Initialize DXC
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_d3d->shader_compiler.dx_compiler));
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_d3d->shader_compiler.dxc_utils));
		g_d3d->shader_compiler.dxc_utils->CreateDefaultIncludeHandler(&g_d3d->shader_compiler.dxc_include_handler);

		// Initialize Dear ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// ImGui font descriptor handle is gonna be the very last one of the heap, so application handles can start at 0
		u32 imgui_desc_handle_offset = g_d3d->descriptor_heaps.heap_sizes.cbv_srv_uav - 1;
		D3D12_CPU_DESCRIPTOR_HANDLE imgui_cpu_desc_handle = get_descriptor_heap_cpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, imgui_desc_handle_offset);
		D3D12_GPU_DESCRIPTOR_HANDLE imgui_gpu_desc_handle = get_descriptor_heap_gpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, imgui_desc_handle_offset);

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(g_d3d->device, SWAP_CHAIN_BACK_BUFFER_COUNT, swapchain_desc.Format,
			g_d3d->descriptor_heaps.cbv_srv_uav, imgui_cpu_desc_handle, imgui_gpu_desc_handle);
	}

	void exit()
	{
		LOG_INFO("D3D12", "Exit");

		frame::exit();
		upload::exit();
		descriptor::exit();

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void begin_frame()
	{
		frame_context_t& frame_ctx = get_frame_context();

		// Wait for in-flight frame for the current back buffer
		wait_on_fence(g_d3d->sync.fence, frame_ctx.fence_value);
		frame::reset();

		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX12_NewFrame();
		ImGui::NewFrame();
	}

	void end_frame()
	{
	}

	void copy_to_back_buffer(ID3D12Resource* src_resource, u32 render_width, u32 render_height)
	{
		frame_context_t& frame_ctx = get_frame_context();

		{
			D3D12_RESOURCE_BARRIER barriers[2] =
			{
				transition_barrier(frame_ctx.backbuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST),
				transition_barrier(src_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
			};
			frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
		}

		u32 bpp = 4;
		D3D12_RESOURCE_DESC src_resource_desc = src_resource->GetDesc();
		D3D12_RESOURCE_DESC dst_resource_desc = frame_ctx.backbuffer->GetDesc();

		ASSERT(src_resource_desc.Width == dst_resource_desc.Width &&
			src_resource_desc.Height == dst_resource_desc.Height);

		D3D12_TEXTURE_COPY_LOCATION src_copy_loc = {};
		src_copy_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_copy_loc.pResource = src_resource;
		src_copy_loc.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION dst_copy_loc = {};
		dst_copy_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_copy_loc.pResource = frame_ctx.backbuffer;
		dst_copy_loc.SubresourceIndex = 0;

		frame_ctx.command_list->CopyTextureRegion(&dst_copy_loc, 0, 0, 0, &src_copy_loc, nullptr);

		{
			D3D12_RESOURCE_BARRIER barriers[1] =
			{
				transition_barrier(src_resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON)
			};
			frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
		}
	}

	void present()
	{
		frame_context_t& frame_ctx = get_frame_context();

		// Transition back buffer to render target state for Dear ImGui
		D3D12_RESOURCE_BARRIER render_target_barrier = transition_barrier(frame_ctx.backbuffer,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
		frame_ctx.command_list->ResourceBarrier(1, &render_target_barrier);

		// render Dear ImGui
		ImGui::Render();

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = get_descriptor_heap_cpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_d3d->swapchain.back_buffer_index);
		frame_ctx.command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
		frame_ctx.command_list->SetDescriptorHeaps(1, &g_d3d->descriptor_heaps.cbv_srv_uav);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), frame_ctx.command_list);

		// Transition back buffer to present state for presentation
		D3D12_RESOURCE_BARRIER present_barrier = transition_barrier(frame_ctx.backbuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		frame_ctx.command_list->ResourceBarrier(1, &present_barrier);

		// Submit command list for current frame
		frame_ctx.command_list->Close();
		ID3D12CommandList* const command_lists[] = { frame_ctx.command_list };
		g_d3d->command_queue_direct->ExecuteCommandLists(1, command_lists);

		// Present
		u32 sync_interval = g_d3d->vsync ? 1 : 0;
		u32 present_flags = g_d3d->swapchain.supports_tearing && !g_d3d->vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		DX_CHECK_HR(g_d3d->swapchain.dxgi_swapchain->Present(sync_interval, present_flags));

		// Signal fence with the value for the current frame, update next available back buffer index
		frame_ctx.fence_value = ++g_d3d->sync.fence_value;
		DX_CHECK_HR(g_d3d->command_queue_direct->Signal(g_d3d->sync.fence, frame_ctx.fence_value));
		g_d3d->swapchain.back_buffer_index = g_d3d->swapchain.dxgi_swapchain->GetCurrentBackBufferIndex();
	}

	void release_object(ID3D12Object* object)
	{
		DX_RELEASE_OBJECT(object);
		object = nullptr;
	}

	ID3D12CommandQueue* create_command_queue(const wchar_t* debug_name, D3D12_COMMAND_LIST_TYPE type)
	{
		D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
		cmd_queue_desc.Type = type;
		cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmd_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		cmd_queue_desc.NodeMask = 0;

		ID3D12CommandQueue* d3d_command_queue = nullptr;
		DX_CHECK_HR(g_d3d->device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&d3d_command_queue)));
		DX_CHECK_HR(d3d_command_queue->SetName(debug_name));

		return d3d_command_queue;
	}

	ID3D12CommandAllocator* create_command_allocator(const wchar_t* debug_name, D3D12_COMMAND_LIST_TYPE type)
	{
		ID3D12CommandAllocator* d3d_command_allocator = nullptr;
		DX_CHECK_HR(g_d3d->device->CreateCommandAllocator(type, IID_PPV_ARGS(&d3d_command_allocator)));
		DX_CHECK_HR(d3d_command_allocator->SetName(debug_name));

		return d3d_command_allocator;
	}

	ID3D12GraphicsCommandList6* create_command_list(const wchar_t* debug_name, ID3D12CommandAllocator* d3d_command_allocator, D3D12_COMMAND_LIST_TYPE type)
	{
		ASSERT(d3d_command_allocator);

		ID3D12GraphicsCommandList6* d3d_command_list = nullptr;
		DX_CHECK_HR(g_d3d->device->CreateCommandList(0, type, d3d_command_allocator, nullptr, IID_PPV_ARGS(&d3d_command_list)));
		DX_CHECK_HR(d3d_command_list->SetName(debug_name));

		return d3d_command_list;
	}

	ID3D12Fence* create_fence(const wchar_t* debug_name, u64 initial_fence_value)
	{
		ID3D12Fence* d3d_fence = nullptr;
		DX_CHECK_HR(g_d3d->device->CreateFence(initial_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d_fence)));
		DX_CHECK_HR(d3d_fence->SetName(debug_name));

		return d3d_fence;
	}

	void wait_on_fence(ID3D12Fence* fence, u64 wait_on_fence_value)
	{
		ASSERT(fence);

		if (fence->GetCompletedValue() < wait_on_fence_value)
		{
			DX_CHECK_HR(fence->SetEventOnCompletion(wait_on_fence_value, NULL));
		}
	}

	IDxcBlob* compile_shader(const wchar_t* filepath, const wchar_t* entry_point, const wchar_t* target_profile)
	{
		HRESULT hr = 0;
		u32 codepage = 0;

		IDxcBlobEncoding* shader_source_blob = nullptr;
		hr = g_d3d->shader_compiler.dxc_utils->LoadFile(filepath, &codepage, &shader_source_blob);
		
		if (FAILED(hr))
		{
			FATAL_ERROR("D3D12", "Failed to load shader source: %s", filepath);
			return nullptr;
		}

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
			L"-HV 2021", // Compile with HLSL 2021
			//L"-D <macro>", Defines a macro, macros with values can be defined like this: -D MacroName=MacroValue
			//L"-enable-16bit-types"
			DXC_ARG_WARNINGS_ARE_ERRORS,
			DXC_ARG_OPTIMIZATION_LEVEL3,
			DXC_ARG_PACK_MATRIX_ROW_MAJOR,
#ifdef _DEBUG
			DXC_ARG_DEBUG,
			DXC_ARG_SKIP_OPTIMIZATIONS,
			// Embed PDB inside the shader binary
			L"-Qembed_debug"
#endif
		};

		IDxcResult* compile_result = nullptr;
		DX_CHECK_HR(g_d3d->shader_compiler.dx_compiler->Compile(
			&shader_source_buffer, compile_args, ARRAY_SIZE(compile_args),
			g_d3d->shader_compiler.dxc_include_handler, IID_PPV_ARGS(&compile_result))
		);
		DX_CHECK_HR(compile_result->GetStatus(&hr));

		if (FAILED(hr))
		{
			IDxcBlobEncoding* compile_error = nullptr;
			compile_result->GetErrorBuffer(&compile_error);

			FATAL_ERROR("D3D12", reinterpret_cast<char*>(compile_error->GetBufferPointer()));

			DX_RELEASE_OBJECT(compile_error);
			return nullptr;
		}

		IDxcBlob* shader_binary = nullptr;
		IDxcBlobUtf16* shader_name = nullptr;
		DX_CHECK_HR(compile_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_binary), &shader_name));

		if (!shader_binary)
		{
			FATAL_ERROR("D3D12", "Failed to retrieve the shader binary");
		}

		DX_RELEASE_OBJECT(shader_source_blob);
		DX_RELEASE_OBJECT(shader_name);
		DX_RELEASE_OBJECT(compile_result);

		return shader_binary;
	}

	ID3D12RootSignature* create_root_signature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& root_signature_desc)
	{
		ID3DBlob* root_signature_serialized = nullptr;
		ID3DBlob* error_blob = nullptr;

		DX_CHECK_HR(D3D12SerializeVersionedRootSignature(&root_signature_desc, &root_signature_serialized, &error_blob));
		if (error_blob)
		{
			FATAL_ERROR("D3D12", reinterpret_cast<char*>(error_blob->GetBufferPointer()));
		}
		DX_RELEASE_OBJECT(error_blob);

		ID3D12RootSignature* root_signature = nullptr;
		DX_CHECK_HR(g_d3d->device->CreateRootSignature(0, root_signature_serialized->GetBufferPointer(),
			root_signature_serialized->GetBufferSize(), IID_PPV_ARGS(&root_signature)));

		DX_RELEASE_OBJECT(root_signature_serialized);
		return root_signature;
	}

	ID3D12PipelineState* create_pso_cs(IDxcBlob* cs_binary, ID3D12RootSignature* cs_root_sig)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC cs_pipeline_desc = {};
		cs_pipeline_desc.pRootSignature = cs_root_sig;
		cs_pipeline_desc.CS.pShaderBytecode = cs_binary->GetBufferPointer();
		cs_pipeline_desc.CS.BytecodeLength = cs_binary->GetBufferSize();
		cs_pipeline_desc.NodeMask = 0;
		cs_pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ID3D12PipelineState* cs_pipeline_state = nullptr;
		DX_CHECK_HR(g_d3d->device->CreateComputePipelineState(&cs_pipeline_desc, IID_PPV_ARGS(&cs_pipeline_state)));

		DX_RELEASE_OBJECT(cs_binary);

		return cs_pipeline_state;
	}

	void* map_resource(ID3D12Resource* resource, u32 subresource, u64 byte_offset, u64 byte_count)
	{
		ASSERT(resource);

		void* ptr_mapped = nullptr;

		if (byte_count != D3D12_MAP_FULL_RANGE)
		{
			D3D12_RANGE mapped_range = { byte_offset, byte_offset + byte_count };
			DX_CHECK_HR(resource->Map(subresource, &mapped_range, reinterpret_cast<void**>(&ptr_mapped)));
		}
		else
		{
			DX_CHECK_HR(resource->Map(subresource, nullptr, reinterpret_cast<void**>(&ptr_mapped)));
		}

		return ptr_mapped;
	}

	void unmap_resource(ID3D12Resource* resource, u32 subresource, u64 byte_offset, u64 byte_count)
	{
		ASSERT(resource);

		if (byte_count != D3D12_MAP_FULL_RANGE)
		{
			D3D12_RANGE mapped_range = { byte_offset, byte_offset + byte_count };
			resource->Unmap(subresource, &mapped_range);
		}
		else
		{
			resource->Unmap(subresource, nullptr);
		}
	}

}
