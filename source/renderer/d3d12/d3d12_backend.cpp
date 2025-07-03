#include "pch.h"
#include "d3d12_backend.h"
#include "d3d12_common.h"
#include "d3d12_resource.h"
#include "d3d12_descriptor.h"
#include "d3d12_upload.h"
#include "d3d12_frame.h"
#include "d3d12_query.h"

#include "core/logger.h"
#include "core/memory/memory_arena.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "implot/implot.h"
#include "imguizmo/ImGuizmo.h"

// Specify the D3D12 agility SDK version and path
// This will help the D3D12.dll loader pick the right D3D12Core.dll (either the system d3d12core.dll or provided agility dll)
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 715; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace d3d12
{

	d3d12_instance_t* g_d3d = nullptr;

	void init(const init_params_t& init_params)
	{
		LOG_INFO("D3D12", "Init");

		g_d3d = ARENA_BOOTSTRAP(d3d12_instance_t, 0);
		g_d3d->vsync = init_params.vsync;

		// Enable debug layer
#ifdef _DEBUG
		ID3D12Debug* debug_interface = nullptr;
		DX_CHECK_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
		debug_interface->EnableDebugLayer();

		// Enable GPU based validation
#if D3D12_GPU_BASED_VALIDATION
		ID3D12Debug1* debug_interface1 = nullptr;
		DX_CHECK_HR(debug_interface->QueryInterface(IID_PPV_ARGS(&debug_interface1)));
		debug_interface1->SetEnableGPUBasedValidation(true);
#endif
#endif

		uint32_t factory_create_flags = 0;
#ifdef _DEBUG
		factory_create_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		// Create factory
		IDXGIFactory7* dxgi_factory7 = nullptr;
		DX_CHECK_HR(CreateDXGIFactory2(factory_create_flags, IID_PPV_ARGS(&dxgi_factory7)));

		// Select adapter
		D3D_FEATURE_LEVEL d3d_min_feature_level = D3D_FEATURE_LEVEL_12_2;

		IDXGIAdapter1* dxgi_adapter1 = nullptr;
		uint64_t max_dedicated_video_mem = 0;

		for (uint32_t adapter_idx = 0; dxgi_factory7->EnumAdapters1(adapter_idx, &dxgi_adapter1) != DXGI_ERROR_NOT_FOUND; ++adapter_idx)
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
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS3 d3d_options3 = {};
			DX_CHECK_HR(g_d3d->device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &d3d_options3, sizeof(d3d_options3)));

			if (!d3d_options3.CopyQueueTimestampQueriesSupported)
				FATAL_ERROR("D3D12", "DirectX 12 Feature not supported: Copy Queue Timestamp Queries");
			
			D3D12_FEATURE_DATA_D3D12_OPTIONS5 d3d_options5 = {};
			DX_CHECK_HR(g_d3d->device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &d3d_options5, sizeof(d3d_options5)));

			if (d3d_options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
				FATAL_ERROR("D3D12", "DirectX 12 Feature not supported: D3D12 Raytracing Tier 1.0");

			D3D12_FEATURE_DATA_D3D12_OPTIONS12 d3d_options12 = {};
			DX_CHECK_HR(g_d3d->device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &d3d_options12, sizeof(d3d_options12)));

			if (!d3d_options12.EnhancedBarriersSupported)
				FATAL_ERROR("D3D12", "DirectX 12 Feature not supported: Enhanced Barriers");
		}

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
		g_d3d->swapchain.back_buffer_count = init_params.back_buffer_count;

		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
		swapchain_desc.Width = g_d3d->swapchain.output_width;
		swapchain_desc.Height = g_d3d->swapchain.output_height;
		swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_desc.Stereo = FALSE;
		swapchain_desc.SampleDesc = { 1, 0 };
		swapchain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		swapchain_desc.BufferCount = g_d3d->swapchain.back_buffer_count;
		swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
		swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapchain_desc.Flags = g_d3d->swapchain.supports_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		IDXGISwapChain1* dxgi_swapchain1 = nullptr;
		DX_CHECK_HR(dxgi_factory7->CreateSwapChainForHwnd(g_d3d->command_queue_direct, hwnd, &swapchain_desc, nullptr, nullptr, &dxgi_swapchain1));
		DX_CHECK_HR(dxgi_swapchain1->QueryInterface(&g_d3d->swapchain.dxgi_swapchain));
		DX_CHECK_HR(dxgi_factory7->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		g_d3d->swapchain.back_buffer_index = g_d3d->swapchain.dxgi_swapchain->GetCurrentBackBufferIndex();

		// Create fence and fence event
		g_d3d->sync.fence = create_fence(L"Frame Fence");
		g_d3d->sync.fence_value = 0ull;

		// Initialize descriptor heaps, descriptor allocation
		g_d3d->descriptor_heaps.heap_sizes.rtv = g_d3d->swapchain.back_buffer_count;
		g_d3d->descriptor_heaps.heap_sizes.cbv_srv_uav = 1024;
		init_descriptors();

		g_d3d->immediate.command_allocator = create_command_allocator(L"Immediate Command Allocator", D3D12_COMMAND_LIST_TYPE_DIRECT);
		g_d3d->immediate.command_list = create_command_list(L"Immediate Command List", g_d3d->immediate.command_allocator, D3D12_COMMAND_LIST_TYPE_DIRECT);
		g_d3d->immediate.fence = create_fence(L"Immediate Fence");
		g_d3d->immediate.fence_value = 0ull;

		// Initialize upload ring buffer
		init_upload_context(UPLOAD_BUFFER_DEFAULT_CAPACITY);

		// Initialize per-frame contexts
		init_frame_contexts(FRAME_ALLOCATOR_DEFAULT_CAPACITY, swapchain_desc);

		// Initialize queries and query heaps
		init_queries(TIMESTAMP_QUERIES_DEFAULT_CAPACITY);

		// Initialize DXC
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_d3d->shader_compiler.dx_compiler));
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_d3d->shader_compiler.dxc_utils));
		g_d3d->shader_compiler.dxc_utils->CreateDefaultIncludeHandler(&g_d3d->shader_compiler.dxc_include_handler);

		// Initialize Dear ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// ImGui font descriptor handle is gonna be the very last one of the heap, so application handles can start at 0
		uint32_t imgui_desc_handle_offset = g_d3d->descriptor_heaps.heap_sizes.cbv_srv_uav - 1;
		D3D12_CPU_DESCRIPTOR_HANDLE imgui_cpu_desc_handle = get_descriptor_heap_cpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, imgui_desc_handle_offset);
		D3D12_GPU_DESCRIPTOR_HANDLE imgui_gpu_desc_handle = get_descriptor_heap_gpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, imgui_desc_handle_offset);

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(g_d3d->device, g_d3d->swapchain.back_buffer_count, swapchain_desc.Format,
			g_d3d->descriptor_heaps.cbv_srv_uav, imgui_cpu_desc_handle, imgui_gpu_desc_handle);
	}

	void exit()
	{
		LOG_INFO("D3D12", "Exit");

		exit_queries();
		destroy_frame_contexts();
		destroy_upload_context();
		exit_descriptors();

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	}

	void begin_frame()
	{
		frame_context_t& frame_ctx = get_frame_context();

		// Wait for in-flight frame for the current back buffer
		wait_on_fence(g_d3d->sync.fence, frame_ctx.fence_value);
		reset_frame_context();
		reset_queries();

		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX12_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void end_frame()
	{
	}

	void copy_to_back_buffer(ID3D12Resource* src_resource, uint32_t render_width, uint32_t render_height)
	{
		frame_context_t& frame_ctx = get_frame_context();

		{
			D3D12_RESOURCE_BARRIER barriers[2] =
			{
				barrier_transition(frame_ctx.backbuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST),
				barrier_transition(src_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
			};
			frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
		}

		uint32_t bpp = 4;
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
				barrier_transition(src_resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON)
			};
			frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
		}
	}

	void render_imgui()
	{
		frame_context_t& frame_ctx = get_frame_context();

		// Transition back buffer to render target state for Dear ImGui
		{
			D3D12_RESOURCE_BARRIER barriers[1] =
			{
				barrier_transition(frame_ctx.backbuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET)
			};
			frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
		}

		// Render Dear ImGui
		ImGui::Render();

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = get_descriptor_heap_cpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_d3d->swapchain.back_buffer_index);
		frame_ctx.command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
		frame_ctx.command_list->SetDescriptorHeaps(1, &g_d3d->descriptor_heaps.cbv_srv_uav);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), frame_ctx.command_list);
	}

	void present()
	{
		frame_context_t& frame_ctx = get_frame_context();

		// Transition back buffer to present state for presentation
		{
			D3D12_RESOURCE_BARRIER barriers[1] =
			{
				barrier_transition(frame_ctx.backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)
			};
			frame_ctx.command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
		}

		// Before we close the command list we want to resolve any timestamp queries issued on the command list
		resolve_timestamp_queries(frame_ctx.command_list);

		// Submit command list for current frame
		frame_ctx.command_list->Close();
		ID3D12CommandList* const command_lists[] = { frame_ctx.command_list };
		g_d3d->command_queue_direct->ExecuteCommandLists(1, command_lists);

		// Present
		uint32_t sync_interval = g_d3d->vsync ? 1 : 0;
		uint32_t present_flags = g_d3d->swapchain.supports_tearing && !g_d3d->vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		DX_CHECK_HR(g_d3d->swapchain.dxgi_swapchain->Present(sync_interval, present_flags));

		// Signal fence with the value for the current frame, update next available back buffer index
		frame_ctx.fence_value = ++g_d3d->sync.fence_value;
		DX_CHECK_HR(g_d3d->command_queue_direct->Signal(g_d3d->sync.fence, frame_ctx.fence_value));
		g_d3d->swapchain.back_buffer_index = g_d3d->swapchain.dxgi_swapchain->GetCurrentBackBufferIndex();
	}

	void flush()
	{
		// Wait on in-flight frames to finish execution
		if (g_d3d->sync.fence->GetCompletedValue() < g_d3d->sync.fence_value)
		{
			wait_on_fence(g_d3d->sync.fence, g_d3d->sync.fence_value);
		}

		for (uint32_t i = 0; i < g_d3d->swapchain.back_buffer_count; ++i)
		{
			if (g_d3d->sync.fence->GetCompletedValue() < g_d3d->frame_ctx[i].fence_value)
			{
				wait_on_fence(g_d3d->sync.fence, g_d3d->frame_ctx[i].fence_value);
			}
		}

		// Wait on immediate command lists to finish execution
		if (g_d3d->immediate.fence->GetCompletedValue() < g_d3d->immediate.fence_value)
		{
			wait_on_fence(g_d3d->immediate.fence, g_d3d->immediate.fence_value);
		}

		// Flush in-flight uploads
		flush_uploads();
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

	ID3D12GraphicsCommandList10* create_command_list(const wchar_t* debug_name, ID3D12CommandAllocator* d3d_command_allocator, D3D12_COMMAND_LIST_TYPE type)
	{
		ASSERT(d3d_command_allocator);

		ID3D12GraphicsCommandList10* d3d_command_list = nullptr;
		DX_CHECK_HR(g_d3d->device->CreateCommandList(0, type, d3d_command_allocator, nullptr, IID_PPV_ARGS(&d3d_command_list)));
		DX_CHECK_HR(d3d_command_list->SetName(debug_name));

		return d3d_command_list;
	}

	ID3D12Fence* create_fence(const wchar_t* debug_name, uint64_t initial_fence_value)
	{
		ID3D12Fence* d3d_fence = nullptr;
		DX_CHECK_HR(g_d3d->device->CreateFence(initial_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d_fence)));
		DX_CHECK_HR(d3d_fence->SetName(debug_name));

		return d3d_fence;
	}

	void wait_on_fence(ID3D12Fence* fence, uint64_t wait_on_fence_value)
	{
		ASSERT(fence);

		if (fence->GetCompletedValue() < wait_on_fence_value)
		{
			DX_CHECK_HR(fence->SetEventOnCompletion(wait_on_fence_value, NULL));
		}
	}

	IDxcBlob* compile_shader(const wchar_t* filepath, const wchar_t* entry_point, const wchar_t* target_profile,
		uint32_t define_count, const DxcDefine* defines)
	{
		HRESULT hr = 0;
		uint32_t codepage = 0;

		IDxcBlobEncoding* shader_source_blob = nullptr;
		hr = g_d3d->shader_compiler.dxc_utils->LoadFile(filepath, &codepage, &shader_source_blob);
		
		if (FAILED(hr))
		{
			FATAL_ERROR("D3D12", "Failed to load shader source: %ls", filepath);
			return nullptr;
		}

		DxcBuffer shader_source_buffer = {};
		shader_source_buffer.Encoding = DXC_CP_ACP;
		shader_source_buffer.Ptr = shader_source_blob->GetBufferPointer();
		shader_source_buffer.Size = shader_source_blob->GetBufferSize();

		IDxcCompilerArgs* compiler_args = nullptr;
		const wchar_t* args[] =
		{
			filepath,
			L"-E", entry_point,
			L"-T", target_profile,
			L"-HV 2021", //L"-enable-16bit-types",
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
		g_d3d->shader_compiler.dxc_utils->BuildArguments(filepath, entry_point, target_profile, args, ARRAY_SIZE(args), defines, define_count, &compiler_args);

		IDxcResult* compile_result = nullptr;
		DX_CHECK_HR(g_d3d->shader_compiler.dx_compiler->Compile(
			&shader_source_buffer, compiler_args->GetArguments(), compiler_args->GetCount(),
			g_d3d->shader_compiler.dxc_include_handler, IID_PPV_ARGS(&compile_result))
		);
		DX_CHECK_HR(compile_result->GetStatus(&hr));

		if (FAILED(hr))
		{
			IDxcBlobEncoding* compile_error = nullptr;
			compile_result->GetErrorBuffer(&compile_error);

			// compiler_args->GetArguments() does not want to work, because its an array of const wchar_t instead of just one contiguous string
			/*FATAL_ERROR("D3D12", "Tried to compile shader %ls (entry: %ls, target: %ls) with args %ls. Error: %s",
				filepath, entry_point, target_profile, compiler_args->GetArguments(), (char*)compile_error->GetBufferPointer());*/
			FATAL_ERROR("D3D12", "Failed shader compilation.\nShader: %ls\nEntry point: %ls\nTarget profile: %ls\nArguments: \nError: %s",
				filepath, entry_point, target_profile, /*(const wchar_t*)compiler_args->GetArguments(), */(char*)compile_error->GetBufferPointer());

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
			FATAL_ERROR("D3D12", (char*)error_blob->GetBufferPointer());
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

	void* map_resource(ID3D12Resource* resource, uint32_t subresource, uint64_t byte_offset, uint64_t byte_count)
	{
		ASSERT(resource);

		void* ptr_mapped = nullptr;

		if (byte_count != D3D12_MAP_FULL_RANGE)
		{
			D3D12_RANGE mapped_range = { byte_offset, byte_offset + byte_count };
			DX_CHECK_HR(resource->Map(subresource, &mapped_range, (void**)&ptr_mapped));
		}
		else
		{
			DX_CHECK_HR(resource->Map(subresource, nullptr, (void**)&ptr_mapped));
		}

		return ptr_mapped;
	}

	void unmap_resource(ID3D12Resource* resource, uint32_t subresource, uint64_t byte_offset, uint64_t byte_count)
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

	ID3D12GraphicsCommandList10* get_immediate_command_list()
	{
		if (g_d3d->immediate.fence->GetCompletedValue() < g_d3d->immediate.fence_value)
		{
			wait_on_fence(g_d3d->immediate.fence, g_d3d->immediate.fence_value);
		}

		if (g_d3d->immediate.fence_value > 0)
		{
			g_d3d->immediate.command_allocator->Reset();
			g_d3d->immediate.command_list->Reset(g_d3d->immediate.command_allocator, nullptr);
		}

		return g_d3d->immediate.command_list;
	}

	uint64_t execute_immediate_command_list(ID3D12GraphicsCommandList10* command_list, bool wait_on_finish)
	{
		ASSERT_MSG(g_d3d->immediate.command_list == command_list, "Command list passed into function is not an immediate command list");

		// Do the actual stuff
		command_list->Close();
		ID3D12CommandList* const command_lists[] = { command_list };
		g_d3d->command_queue_direct->ExecuteCommandLists(1, command_lists);

		g_d3d->immediate.fence_value++;
		DX_CHECK_HR(g_d3d->command_queue_direct->Signal(g_d3d->immediate.fence, g_d3d->immediate.fence_value));

		if (wait_on_finish)
		{
			wait_on_fence(g_d3d->immediate.fence, g_d3d->immediate.fence_value);
		}

		return g_d3d->immediate.fence_value;
	}

	device_memory_info_t get_device_memory_info()
	{
		device_memory_info_t memory_info = {};
		g_d3d->dxgi_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory_info.local_mem);
		g_d3d->dxgi_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &memory_info.non_local_mem);

		return memory_info;
	}

	DXGI_FORMAT get_dxgi_texture_format(TEXTURE_FORMAT format)
	{
		switch (format)
		{
		case TEXTURE_FORMAT_RG8:			return DXGI_FORMAT_R8G8_UNORM;
		case TEXTURE_FORMAT_RGBA8:			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case TEXTURE_FORMAT_RGBA8_SRGB:		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case TEXTURE_FORMAT_RGBA32_FLOAT:	return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default:							return DXGI_FORMAT_UNKNOWN;
		}
	}

}
