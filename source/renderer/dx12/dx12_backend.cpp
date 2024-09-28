#include "pch.h"
#include "dx12_backend.h"
#include "dx12_common.h"
#include "dx12_resource.h"
#include "dx12_descriptor.h"
#include "core/logger.h"
#include "core/memory/memory_arena.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

// Specify the D3D12 agility SDK version and path
// This will help the D3D12.dll loader pick the right D3D12Core.dll (either the system g_dx12alled or provided agility)
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 715; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace dx12_backend
{

	dx12_instance_t* g_dx12 = nullptr;
	
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
		LOG_INFO("DX12 Backend", "Init");

		g_dx12 = ARENA_ALLOC_STRUCT_ZERO(arena, dx12_instance_t);
		g_dx12->arena = arena;

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
				DX_CHECK_HR(dxgi_adapter1->QueryInterface(IID_PPV_ARGS(&g_dx12->dxgi_adapter)));
			}
		}

		// Create D3D12 device
		DX_CHECK_HR(D3D12CreateDevice(g_dx12->dxgi_adapter, d3d_min_feature_level, IID_PPV_ARGS(&g_dx12->d3d12_device)));

		// Check if additional required features are supported
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 d3d_options12 = {};
		DX_CHECK_HR(g_dx12->d3d12_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &d3d_options12, sizeof(d3d_options12)));

		if (!d3d_options12.EnhancedBarriersSupported)
			FATAL_ERROR("DX12 Backend", "DirectX 12 Feature not supported: Enhanced Barriers");

#ifdef _DEBUG
		// Set info queue behavior and filters
		ID3D12InfoQueue* d3d12_info_queue = nullptr;
		DX_CHECK_HR(g_dx12->d3d12_device->QueryInterface(IID_PPV_ARGS(&d3d12_info_queue)));
		DX_CHECK_HR(d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
		DX_CHECK_HR(d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
		DX_CHECK_HR(d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
#endif

		// Check for tearing support
		BOOL allow_tearing = FALSE;
		DX_CHECK_HR(dxgi_factory7->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(BOOL)));
		g_dx12->swapchain.supports_tearing = (allow_tearing == TRUE);

		// Create swap chain command queue
		D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
		cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmd_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		cmd_queue_desc.NodeMask = 0;
		DX_CHECK_HR(g_dx12->d3d12_device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&g_dx12->d3d12_command_queue_direct)));

		// Create swap chain
		HWND hwnd = GetActiveWindow();

		RECT window_rect = {};
		GetClientRect(hwnd, &window_rect);

		g_dx12->swapchain.output_width = window_rect.right - window_rect.left;
		g_dx12->swapchain.output_height = window_rect.bottom - window_rect.top;

		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
		swapchain_desc.Width = g_dx12->swapchain.output_width;
		swapchain_desc.Height = g_dx12->swapchain.output_height;
		swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_desc.Stereo = FALSE;
		swapchain_desc.SampleDesc = { 1, 0 };
		swapchain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		swapchain_desc.BufferCount = SWAP_CHAIN_BACK_BUFFER_COUNT;
		swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
		swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapchain_desc.Flags = g_dx12->swapchain.supports_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		IDXGISwapChain1* dxgi_swapchain1 = nullptr;
		DX_CHECK_HR(dxgi_factory7->CreateSwapChainForHwnd(g_dx12->d3d12_command_queue_direct, hwnd, &swapchain_desc, nullptr, nullptr, &dxgi_swapchain1));
		DX_CHECK_HR(dxgi_swapchain1->QueryInterface(&g_dx12->swapchain.dxgi_swapchain));
		DX_CHECK_HR(dxgi_factory7->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		g_dx12->swapchain.back_buffer_index = g_dx12->swapchain.dxgi_swapchain->GetCurrentBackBufferIndex();

		// Create command allocators and command lists
		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			DX_CHECK_HR(g_dx12->d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_dx12->frame_ctx[i].d3d12_command_allocator)));
			DX_CHECK_HR(g_dx12->d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				g_dx12->frame_ctx[i].d3d12_command_allocator, nullptr, IID_PPV_ARGS(&g_dx12->frame_ctx[i].d3d12_command_list)));
		}

		// Create fence and fence event
		DX_CHECK_HR(g_dx12->d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_dx12->sync.d3d12_fence)));

		// Initialize descriptor heaps, descriptor allocation
		g_dx12->descriptor_heaps.heap_sizes.rtv = SWAP_CHAIN_BACK_BUFFER_COUNT;
		g_dx12->descriptor_heaps.heap_sizes.cbv_srv_uav = 1024;
		descriptor::init();

		// Create render target views for all back buffers
		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			frame_context_t& frame_ctx = g_dx12->frame_ctx[i];

			g_dx12->swapchain.dxgi_swapchain->GetBuffer(i, IID_PPV_ARGS(&frame_ctx.backbuffer));
			frame_ctx.backbuffer_rtv = descriptor::alloc(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			create_texture_2d_rtv(frame_ctx.backbuffer, frame_ctx.backbuffer_rtv, 0, swapchain_desc.Format);
		}

		// Initialize DXC
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_dx12->shader_compiler.dx_compiler));
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_dx12->shader_compiler.dxc_utils));
		g_dx12->shader_compiler.dxc_utils->CreateDefaultIncludeHandler(&g_dx12->shader_compiler.dxc_include_handler);

		// Initialize Dear ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// ImGui font descriptor handle is gonna be the very last one of the heap, so application handles can start at 0
		u32 imgui_desc_handle_offset = g_dx12->descriptor_heaps.heap_sizes.cbv_srv_uav - 1;
		D3D12_CPU_DESCRIPTOR_HANDLE imgui_cpu_desc_handle = get_descriptor_heap_cpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, imgui_desc_handle_offset);
		D3D12_GPU_DESCRIPTOR_HANDLE imgui_gpu_desc_handle = get_descriptor_heap_gpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, imgui_desc_handle_offset);

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(g_dx12->d3d12_device, SWAP_CHAIN_BACK_BUFFER_COUNT, swapchain_desc.Format,
			g_dx12->descriptor_heaps.cbv_srv_uav, imgui_cpu_desc_handle, imgui_gpu_desc_handle);
	}

	void exit()
	{
		LOG_INFO("DX12 Backend", "Exit");

		descriptor::exit();

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void begin_frame()
	{
		frame_context_t& frame_ctx = get_frame_context();

		// Wait for in-flight frame for the current back buffer
		if (g_dx12->sync.d3d12_fence->GetCompletedValue() < frame_ctx.fence_value)
		{
			DX_CHECK_HR(g_dx12->sync.d3d12_fence->SetEventOnCompletion(frame_ctx.fence_value, NULL));
		}

		// reset command allocator and command list for the next back buffer
		if (frame_ctx.fence_value > 0)
		{
			DX_CHECK_HR(frame_ctx.d3d12_command_allocator->Reset());
			DX_CHECK_HR(frame_ctx.d3d12_command_list->Reset(frame_ctx.d3d12_command_allocator, nullptr));
		}

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
			frame_ctx.d3d12_command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
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

		frame_ctx.d3d12_command_list->CopyTextureRegion(&dst_copy_loc, 0, 0, 0, &src_copy_loc, nullptr);

		{
			D3D12_RESOURCE_BARRIER barriers[1] =
			{
				transition_barrier(src_resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON)
			};
			frame_ctx.d3d12_command_list->ResourceBarrier(ARRAY_SIZE(barriers), barriers);
		}
	}

	void present()
	{
		frame_context_t& frame_ctx = get_frame_context();

		// Transition back buffer to render target state for Dear ImGui
		D3D12_RESOURCE_BARRIER render_target_barrier = transition_barrier(frame_ctx.backbuffer,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
		frame_ctx.d3d12_command_list->ResourceBarrier(1, &render_target_barrier);

		// render Dear ImGui
		ImGui::Render();

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = get_descriptor_heap_cpu_handle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_dx12->swapchain.back_buffer_index);
		frame_ctx.d3d12_command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
		frame_ctx.d3d12_command_list->SetDescriptorHeaps(1, &g_dx12->descriptor_heaps.cbv_srv_uav);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), frame_ctx.d3d12_command_list);

		// Transition back buffer to present state for presentation
		D3D12_RESOURCE_BARRIER present_barrier = transition_barrier(frame_ctx.backbuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		frame_ctx.d3d12_command_list->ResourceBarrier(1, &present_barrier);

		// Submit command list for current frame
		frame_ctx.d3d12_command_list->Close();
		ID3D12CommandList* const command_lists[] = { frame_ctx.d3d12_command_list };
		g_dx12->d3d12_command_queue_direct->ExecuteCommandLists(1, command_lists);

		// present
		u32 sync_interval = g_dx12->vsync ? 1 : 0;
		u32 present_flags = g_dx12->swapchain.supports_tearing && !g_dx12->vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		DX_CHECK_HR(g_dx12->swapchain.dxgi_swapchain->Present(sync_interval, present_flags));

		// Signal fence with the value for the current frame, update next available back buffer index
		frame_ctx.fence_value = ++g_dx12->sync.fence_value;
		DX_CHECK_HR(g_dx12->d3d12_command_queue_direct->Signal(g_dx12->sync.d3d12_fence, frame_ctx.fence_value));
		g_dx12->swapchain.back_buffer_index = g_dx12->swapchain.dxgi_swapchain->GetCurrentBackBufferIndex();
	}

}
