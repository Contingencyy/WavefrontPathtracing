#include "pch.h"
#include "dx12_backend.h"
#include "core/common.h"
#include "core/logger.h"
#include "core/memory/memory_arena.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#ifdef create_window
#undef create_window
#endif

#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

#ifdef LoadImage
#undef LoadImage
#endif

#ifdef OPAQUE
#undef OPAQUE
#endif

#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

static inline const char* get_hr_message(HRESULT HR)
{
	char* msg = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, HR, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&msg, 0, NULL);
	return msg;
}

static inline void dx_check_hr(i32 Line, const char* File, HRESULT HR)
{
	if (FAILED(HR))
		fatal_error(Line, File, "DX12 Backend", get_hr_message(HR));
}

#define DX_CHECK_HR(hr) dx_check_hr(__LINE__, __FILE__, hr)

namespace dx12_backend
{

	static constexpr u32 SWAP_CHAIN_BACK_BUFFER_COUNT = 2u;

	struct frame_context_t
	{
		ID3D12CommandAllocator* d3d12_command_allocator = nullptr;
		ID3D12GraphicsCommandList6* d3d12_command_list = nullptr;

		ID3D12Resource* backbuffer = nullptr;
		ID3D12Resource* upload_buffer = nullptr;
		u8* ptr_upload_buffer = nullptr;

		u64 fence_value = 0;
	};

	struct instance_t
	{
		memory_arena_t* arena = nullptr;

		IDXGIAdapter4* dxgi_adapter = nullptr;
		ID3D12Device8* d3d12_device = nullptr;
		ID3D12CommandQueue* d3d12_command_queue_direct = nullptr;

		b8 vsync = false;

		frame_context_t frame_ctx[SWAP_CHAIN_BACK_BUFFER_COUNT];

		struct swapchain_t
		{
			IDXGISwapChain4* dxgi_swapchain = nullptr;
			u32 back_buffer_index = 0u;
			b8 supports_tearing = false;

			u32 output_width = 0;
			u32 output_height = 0;
		} swapchain;

		struct sync_t
		{
			ID3D12Fence* d3d12_fence = nullptr;
			u64 fence_value = 0ull;
		} sync;

		struct descriptor_heaps_t
		{
			ID3D12DescriptorHeap* rtv = nullptr;
			ID3D12DescriptorHeap* cbv_srv_uav = nullptr;
		} descriptor_heaps;
	} static *inst;

	static inline frame_context_t& get_frame_context()
	{
		return inst->frame_ctx[inst->swapchain.back_buffer_index];
	}
	
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
		LOG_INFO("dx12_backend", "init");

		inst = ARENA_ALLOC_STRUCT_ZERO(arena, instance_t);
		inst->arena = arena;

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
		D3D_FEATURE_LEVEL d3d_min_feature_level = D3D_FEATURE_LEVEL_12_1;
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
				DX_CHECK_HR(dxgi_adapter1->QueryInterface(IID_PPV_ARGS(&inst->dxgi_adapter)));
			}
		}

		// Create D3D12 device
		DX_CHECK_HR(D3D12CreateDevice(inst->dxgi_adapter, d3d_min_feature_level, IID_PPV_ARGS(&inst->d3d12_device)));

#ifdef _DEBUG
		// Set info queue behavior and filters
		ID3D12InfoQueue* d3d12_info_queue = nullptr;
		DX_CHECK_HR(Inst->D3D12Device->QueryInterface(IID_PPV_ARGS(&d3d12_info_queue)));
		DX_CHECK_HR(d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
		DX_CHECK_HR(d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
		DX_CHECK_HR(d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
#endif

		// Check for tearing support
		BOOL allow_tearing = FALSE;
		DX_CHECK_HR(dxgi_factory7->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(BOOL)));
		inst->swapchain.supports_tearing = (allow_tearing == TRUE);

		// Create swap chain command queue
		D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
		cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmd_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		cmd_queue_desc.NodeMask = 0;
		DX_CHECK_HR(inst->d3d12_device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&inst->d3d12_command_queue_direct)));

		// Create swap chain
		HWND hwnd = GetActiveWindow();

		RECT window_rect = {};
		GetClientRect(hwnd, &window_rect);

		inst->swapchain.output_width = window_rect.right - window_rect.left;
		inst->swapchain.output_height = window_rect.bottom - window_rect.top;

		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
		swapchain_desc.Width = inst->swapchain.output_width;
		swapchain_desc.Height = inst->swapchain.output_height;
		swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_desc.Stereo = FALSE;
		swapchain_desc.SampleDesc = { 1, 0 };
		swapchain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		swapchain_desc.BufferCount = SWAP_CHAIN_BACK_BUFFER_COUNT;
		swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
		swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapchain_desc.Flags = inst->swapchain.supports_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		IDXGISwapChain1* dxgi_swapchain1 = nullptr;
		DX_CHECK_HR(dxgi_factory7->CreateSwapChainForHwnd(inst->d3d12_command_queue_direct, hwnd, &swapchain_desc, nullptr, nullptr, &dxgi_swapchain1));
		DX_CHECK_HR(dxgi_swapchain1->QueryInterface(&inst->swapchain.dxgi_swapchain));
		DX_CHECK_HR(dxgi_factory7->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		inst->swapchain.back_buffer_index = inst->swapchain.dxgi_swapchain->GetCurrentBackBufferIndex();

		// Create command allocators and command lists
		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			DX_CHECK_HR(inst->d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&inst->frame_ctx[i].d3d12_command_allocator)));
			DX_CHECK_HR(inst->d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				inst->frame_ctx[i].d3d12_command_allocator, nullptr, IID_PPV_ARGS(&inst->frame_ctx[i].d3d12_command_list)));
		}

		// Create fence and fence event
		DX_CHECK_HR(inst->d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&inst->sync.d3d12_fence)));

		// Create upload buffer
		D3D12_HEAP_PROPERTIES upload_heap_props = {};
		upload_heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC upload_resource_desc = {};
		upload_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		upload_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		upload_resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		upload_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		// Need to make sure that the upload buffer matches the back buffer it will copy to
		upload_resource_desc.Width = ALIGN_UP_POW2(inst->swapchain.output_width * 4, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT) * inst->swapchain.output_height;
		upload_resource_desc.Height = 1;
		upload_resource_desc.DepthOrArraySize = 1;
		upload_resource_desc.MipLevels = 1;
		upload_resource_desc.SampleDesc.Count = 1;
		upload_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			DX_CHECK_HR(inst->d3d12_device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE,
				&upload_resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&inst->frame_ctx[i].upload_buffer)));
			inst->frame_ctx[i].upload_buffer->Map(0, nullptr, reinterpret_cast<void**>(&inst->frame_ctx[i].ptr_upload_buffer));
		}

		// Create descriptor heaps
		D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
		descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		descriptor_heap_desc.NumDescriptors = SWAP_CHAIN_BACK_BUFFER_COUNT;
		descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		descriptor_heap_desc.NodeMask = 0;
		DX_CHECK_HR(inst->d3d12_device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&inst->descriptor_heaps.rtv)));

		descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptor_heap_desc.NumDescriptors = 1;
		descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DX_CHECK_HR(inst->d3d12_device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&inst->descriptor_heaps.cbv_srv_uav)));

		// Create render target views for all back buffers
		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			inst->swapchain.dxgi_swapchain->GetBuffer(i, IID_PPV_ARGS(&inst->frame_ctx[i].backbuffer));

			D3D12_RENDER_TARGET_VIEW_DESC back_buffer_rtv_desc = {};
			back_buffer_rtv_desc.Format = swapchain_desc.Format;
			back_buffer_rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			back_buffer_rtv_desc.Texture2D.MipSlice = 0;
			back_buffer_rtv_desc.Texture2D.PlaneSlice = 0;

			u32 rtv_size = inst->d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			inst->d3d12_device->CreateRenderTargetView(inst->frame_ctx[i].backbuffer, &back_buffer_rtv_desc,
				{ inst->descriptor_heaps.rtv->GetCPUDescriptorHandleForHeapStart().ptr + i * rtv_size });
		}

		// Initialize Dear ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(inst->d3d12_device, SWAP_CHAIN_BACK_BUFFER_COUNT, swapchain_desc.Format, inst->descriptor_heaps.cbv_srv_uav,
			inst->descriptor_heaps.cbv_srv_uav->GetCPUDescriptorHandleForHeapStart(), inst->descriptor_heaps.cbv_srv_uav->GetGPUDescriptorHandleForHeapStart());
	}

	void exit()
	{
		LOG_INFO("dx12_backend", "exit");

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			inst->frame_ctx[i].upload_buffer->Unmap(0, nullptr);
		}
	}

	void begin_frame()
	{
		frame_context_t& frame_ctx = get_frame_context();

		// Wait for in-flight frame for the current back buffer
		if (inst->sync.d3d12_fence->GetCompletedValue() < frame_ctx.fence_value)
		{
			DX_CHECK_HR(inst->sync.d3d12_fence->SetEventOnCompletion(frame_ctx.fence_value, NULL));
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

	void copy_to_back_buffer(u8* ptr_pixel_data)
	{
		frame_context_t& frame_ctx = get_frame_context();

		D3D12_RESOURCE_BARRIER copy_dst_barrier = transition_barrier(frame_ctx.backbuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
		frame_ctx.d3d12_command_list->ResourceBarrier(1, &copy_dst_barrier);

		u32 bpp = 4;
		D3D12_RESOURCE_DESC dst_resource_desc = frame_ctx.backbuffer->GetDesc();

		D3D12_SUBRESOURCE_FOOTPRINT dst_footprint = {};
		dst_footprint.Format = dst_resource_desc.Format;
		dst_footprint.Width = static_cast<UINT>(dst_resource_desc.Width);
		dst_footprint.Height = dst_resource_desc.Height;
		dst_footprint.Depth = 1;
		dst_footprint.RowPitch = ALIGN_UP_POW2(static_cast<u32>(dst_resource_desc.Width * bpp), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT src_footprint = {};
		src_footprint.Footprint = dst_footprint;
		src_footprint.Offset = 0;

		u8* ptr_src = ptr_pixel_data;
		u32 src_pitch = dst_resource_desc.Width * bpp;
		u8* ptr_dst = frame_ctx.ptr_upload_buffer;
		u32 dst_pitch = dst_footprint.RowPitch;

		for (u32 y = 0; y < dst_resource_desc.Height; ++y)
		{
			memcpy(ptr_dst, ptr_src, src_pitch);
			ptr_src += src_pitch;
			ptr_dst += dst_pitch;
		}

		D3D12_TEXTURE_COPY_LOCATION src_copy_loc = {};
		src_copy_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src_copy_loc.pResource = frame_ctx.upload_buffer;
		src_copy_loc.PlacedFootprint = src_footprint;

		D3D12_TEXTURE_COPY_LOCATION dst_copy_loc = {};
		dst_copy_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_copy_loc.pResource = frame_ctx.backbuffer;
		dst_copy_loc.SubresourceIndex = 0;

		frame_ctx.d3d12_command_list->CopyTextureRegion(&dst_copy_loc, 0, 0, 0, &src_copy_loc, nullptr);
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

		u32 rtv_size = inst->d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = { inst->descriptor_heaps.rtv->GetCPUDescriptorHandleForHeapStart().ptr +
			inst->swapchain.back_buffer_index * rtv_size };
		frame_ctx.d3d12_command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
		frame_ctx.d3d12_command_list->SetDescriptorHeaps(1, &inst->descriptor_heaps.cbv_srv_uav);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), frame_ctx.d3d12_command_list);

		// Transition back buffer to present state for presentation
		D3D12_RESOURCE_BARRIER present_barrier = transition_barrier(frame_ctx.backbuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		frame_ctx.d3d12_command_list->ResourceBarrier(1, &present_barrier);

		// Submit command list for current frame
		frame_ctx.d3d12_command_list->Close();
		ID3D12CommandList* const command_lists[] = { frame_ctx.d3d12_command_list };
		inst->d3d12_command_queue_direct->ExecuteCommandLists(1, command_lists);

		// present
		u32 sync_interval = inst->vsync ? 1 : 0;
		u32 present_flags = inst->swapchain.supports_tearing && !inst->vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		DX_CHECK_HR(inst->swapchain.dxgi_swapchain->Present(sync_interval, present_flags));

		// Signal fence with the value for the current frame, update next available back buffer index
		frame_ctx.fence_value = ++inst->sync.fence_value;
		DX_CHECK_HR(inst->d3d12_command_queue_direct->Signal(inst->sync.d3d12_fence, frame_ctx.fence_value));
		inst->swapchain.back_buffer_index = inst->swapchain.dxgi_swapchain->GetCurrentBackBufferIndex();
	}

}
