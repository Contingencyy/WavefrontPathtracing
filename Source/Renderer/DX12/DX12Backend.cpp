#include "Pch.h"
#include "DX12Backend.h"
#include "Core/Common.h"
#include "Core/Logger.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#ifdef CreateWindow
#undef CreateWindow
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

static inline const char* GetHRMessage(HRESULT HR)
{
	char* Message = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, HR, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&Message, 0, NULL);
	return Message;
}

static inline void DxCheckHr(i32 Line, const char* File, HRESULT HR)
{
	if (FAILED(HR))
		FatalError(Line, File, "DX12Backend", GetHRMessage(HR));
}

#define DX_CHECK_HR(hr) DxCheckHr(__LINE__, __FILE__, hr)

namespace DX12Backend
{

	static constexpr u32 SWAP_CHAIN_BACK_BUFFER_COUNT = 2u;

	struct FrameContext
	{
		ID3D12CommandAllocator* D3D12CommandAllocator = nullptr;
		ID3D12GraphicsCommandList6* D3D12CommandList = nullptr;

		ID3D12Resource* BackBuffer = nullptr;
		ID3D12Resource* UploadBuffer = nullptr;
		char* PtrUploadBuffer = nullptr;

		u64 FenceValue = 0;
	};

	struct Instance
	{
		IDXGIAdapter4* DXGIAdapter = nullptr;
		ID3D12Device8* D3D12Device = nullptr;
		ID3D12CommandQueue* D3D12CommandQueueDirect = nullptr;

		b8 bVsync = false;

		FrameContext FrameCtx[SWAP_CHAIN_BACK_BUFFER_COUNT];

		struct SwapChain
		{
			IDXGISwapChain4* DXGISwapChain = nullptr;
			u32 BackBufferIndex = 0u;
			b8 bSupportsTearing = false;

			u32 OutputWidth = 0;
			u32 OutputHeight = 0;
		} SwapChain;

		struct Sync
		{
			ID3D12Fence* D3D12Fence = nullptr;
			u64 FenceValue = 0ull;
		} Sync;

		struct DescriptorHeaps
		{
			ID3D12DescriptorHeap* Rtv = nullptr;
			ID3D12DescriptorHeap* CbvSrvUav = nullptr;
		} DescriptorHeaps;
	} static *Inst;

	static inline FrameContext& GetFrameContext()
	{
		return Inst->FrameCtx[Inst->SwapChain.BackBufferIndex];
	}
	
	static D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* Resource, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter)
	{
		D3D12_RESOURCE_BARRIER Barrier = {};
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Transition.pResource = Resource;
		Barrier.Transition.Subresource = 0;
		Barrier.Transition.StateBefore = StateBefore;
		Barrier.Transition.StateAfter = StateAfter;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		return Barrier;
	}

	void Init()
	{
		LOG_INFO("DX12Backend", "Init");

		Inst = new Instance();

		// Enable debug layer
#ifdef _DEBUG
		ID3D12Debug* DebugInterface = nullptr;
		DX_CHECK_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugInterface)));
		DebugInterface->EnableDebugLayer();

		// Enable GPU based validation
#ifdef DX12_GPU_BASED_VALIDATION
		ID3D12Debug1* DebugInterface1 = nullptr;
		DXCheckHR(DebugInterface->QueryInterface(IID_PPV_ARGS(&DebugInterface1)));
		DebugInterface1->SetEnableGPUBasedValidation(true);
#endif
#endif

		u32 FactoryCreateFlags = 0;
#ifdef _DEBUG
		FactoryCreateFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		// Create factory
		IDXGIFactory7* DXGIFactory7 = nullptr;
		DX_CHECK_HR(CreateDXGIFactory2(FactoryCreateFlags, IID_PPV_ARGS(&DXGIFactory7)));

		// Select adapter
		D3D_FEATURE_LEVEL D3DMinFeatureLevel = D3D_FEATURE_LEVEL_12_1;
		IDXGIAdapter1* DXGIAdapter1 = nullptr;
		u64 MaxDedicatedVideoMemory = 0;

		for (u32 AdapterIdx = 0; DXGIFactory7->EnumAdapters1(AdapterIdx, &DXGIAdapter1) != DXGI_ERROR_NOT_FOUND; ++AdapterIdx)
		{
			DXGI_ADAPTER_DESC1 DXGIAdapterDesc = {};
			DX_CHECK_HR(DXGIAdapter1->GetDesc1(&DXGIAdapterDesc));

			if ((DXGIAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(DXGIAdapter1, D3DMinFeatureLevel, __uuidof(ID3D12Device), nullptr)) &&
				DXGIAdapterDesc.DedicatedVideoMemory > MaxDedicatedVideoMemory)
			{
				MaxDedicatedVideoMemory = DXGIAdapterDesc.DedicatedVideoMemory;
				DX_CHECK_HR(DXGIAdapter1->QueryInterface(IID_PPV_ARGS(&Inst->DXGIAdapter)));
			}
		}

		// Create D3D12 device
		DX_CHECK_HR(D3D12CreateDevice(Inst->DXGIAdapter, D3DMinFeatureLevel, IID_PPV_ARGS(&Inst->D3D12Device)));

#ifdef _DEBUG
		// Set info queue behavior and filters
		ID3D12InfoQueue* D3D12InfoQueue = nullptr;
		DX_CHECK_HR(Inst->D3D12Device->QueryInterface(IID_PPV_ARGS(&D3D12InfoQueue)));
		DX_CHECK_HR(D3D12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
		DX_CHECK_HR(D3D12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
		DX_CHECK_HR(D3D12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
#endif

		// Check for tearing support
		BOOL bAllowTearing = FALSE;
		DX_CHECK_HR(DXGIFactory7->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &bAllowTearing, sizeof(BOOL)));
		Inst->SwapChain.bSupportsTearing = (bAllowTearing == TRUE);

		// Create swap chain command queue
		D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
		QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		QueueDesc.NodeMask = 0;
		DX_CHECK_HR(Inst->D3D12Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&Inst->D3D12CommandQueueDirect)));

		// Create swap chain
		HWND hwnd = GetActiveWindow();

		RECT WindowRect = {};
		GetClientRect(hwnd, &WindowRect);

		Inst->SwapChain.OutputWidth = WindowRect.right - WindowRect.left;
		Inst->SwapChain.OutputHeight = WindowRect.bottom - WindowRect.top;

		DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
		SwapChainDesc.Width = Inst->SwapChain.OutputWidth;
		SwapChainDesc.Height = Inst->SwapChain.OutputHeight;
		SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SwapChainDesc.Stereo = FALSE;
		SwapChainDesc.SampleDesc = { 1, 0 };
		SwapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		SwapChainDesc.BufferCount = SWAP_CHAIN_BACK_BUFFER_COUNT;
		SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		SwapChainDesc.Flags = Inst->SwapChain.bSupportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		IDXGISwapChain1* DXGISwapChain1 = nullptr;
		DX_CHECK_HR(DXGIFactory7->CreateSwapChainForHwnd(Inst->D3D12CommandQueueDirect, hwnd, &SwapChainDesc, nullptr, nullptr, &DXGISwapChain1));
		DX_CHECK_HR(DXGISwapChain1->QueryInterface(&Inst->SwapChain.DXGISwapChain));
		DX_CHECK_HR(DXGIFactory7->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		Inst->SwapChain.BackBufferIndex = Inst->SwapChain.DXGISwapChain->GetCurrentBackBufferIndex();

		// Create command allocators and command lists
		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			DX_CHECK_HR(Inst->D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Inst->FrameCtx[i].D3D12CommandAllocator)));
			DX_CHECK_HR(Inst->D3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				Inst->FrameCtx[i].D3D12CommandAllocator, nullptr, IID_PPV_ARGS(&Inst->FrameCtx[i].D3D12CommandList)));
		}

		// Create fence and fence event
		DX_CHECK_HR(Inst->D3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Inst->Sync.D3D12Fence)));

		// Create upload buffer
		D3D12_HEAP_PROPERTIES HeapProperties = {};
		HeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC ResourceDesc = {};
		ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		// Need to make sure that the upload buffer matches the back buffer it will copy to
		ResourceDesc.Width = ALIGN_UP_POW2(Inst->SwapChain.OutputWidth * 4, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT) * Inst->SwapChain.OutputHeight;
		ResourceDesc.Height = 1;
		ResourceDesc.DepthOrArraySize = 1;
		ResourceDesc.MipLevels = 1;
		ResourceDesc.SampleDesc.Count = 1;
		ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			DX_CHECK_HR(Inst->D3D12Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
				&ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Inst->FrameCtx[i].UploadBuffer)));
			Inst->FrameCtx[i].UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&Inst->FrameCtx[i].PtrUploadBuffer));
		}

		// Create descriptor heaps
		D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = {};
		DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		DescriptorHeapDesc.NumDescriptors = SWAP_CHAIN_BACK_BUFFER_COUNT;
		DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DescriptorHeapDesc.NodeMask = 0;
		DX_CHECK_HR(Inst->D3D12Device->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&Inst->DescriptorHeaps.Rtv)));

		DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DescriptorHeapDesc.NumDescriptors = 1;
		DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DX_CHECK_HR(Inst->D3D12Device->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&Inst->DescriptorHeaps.CbvSrvUav)));

		// Create render target views for all back buffers
		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			Inst->SwapChain.DXGISwapChain->GetBuffer(i, IID_PPV_ARGS(&Inst->FrameCtx[i].BackBuffer));

			D3D12_RENDER_TARGET_VIEW_DESC RtvDesc = {};
			RtvDesc.Format = SwapChainDesc.Format;
			RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			RtvDesc.Texture2D.MipSlice = 0;
			RtvDesc.Texture2D.PlaneSlice = 0;

			u32 RtvSize = Inst->D3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			Inst->D3D12Device->CreateRenderTargetView(Inst->FrameCtx[i].BackBuffer, &RtvDesc,
				{ Inst->DescriptorHeaps.Rtv->GetCPUDescriptorHandleForHeapStart().ptr + i * RtvSize });
		}

		// Initialize Dear ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& IO = ImGui::GetIO();
		IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(Inst->D3D12Device, SWAP_CHAIN_BACK_BUFFER_COUNT, SwapChainDesc.Format, Inst->DescriptorHeaps.CbvSrvUav,
			Inst->DescriptorHeaps.CbvSrvUav->GetCPUDescriptorHandleForHeapStart(), Inst->DescriptorHeaps.CbvSrvUav->GetGPUDescriptorHandleForHeapStart());
	}

	void Exit()
	{
		LOG_INFO("DX12Backend", "Exit");

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		for (u32 i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			Inst->FrameCtx[i].UploadBuffer->Unmap(0, nullptr);
		}

		delete Inst;
	}

	void BeginFrame()
	{
		FrameContext& FrameCtx = GetFrameContext();

		// Wait for in-flight frame for the current back buffer
		if (Inst->Sync.D3D12Fence->GetCompletedValue() < FrameCtx.FenceValue)
		{
			DX_CHECK_HR(Inst->Sync.D3D12Fence->SetEventOnCompletion(FrameCtx.FenceValue, NULL));
		}

		// Reset command allocator and command list for the next back buffer
		if (FrameCtx.FenceValue > 0)
		{
			DX_CHECK_HR(FrameCtx.D3D12CommandAllocator->Reset());
			DX_CHECK_HR(FrameCtx.D3D12CommandList->Reset(FrameCtx.D3D12CommandAllocator, nullptr));
		}

		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX12_NewFrame();
		ImGui::NewFrame();
	}

	void EndFrame()
	{
	}

	void CopyToBackBuffer(char* PtrPixelData)
	{
		FrameContext& FrameCtx = GetFrameContext();

		D3D12_RESOURCE_BARRIER CopyDstBarrier = TransitionBarrier(FrameCtx.BackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
		FrameCtx.D3D12CommandList->ResourceBarrier(1, &CopyDstBarrier);

		u32 BytesPerPixel = 4;
		D3D12_RESOURCE_DESC DstResourceDesc = FrameCtx.BackBuffer->GetDesc();

		D3D12_SUBRESOURCE_FOOTPRINT DstFootprint = {};
		DstFootprint.Format = DstResourceDesc.Format;
		DstFootprint.Width = static_cast<UINT>(DstResourceDesc.Width);
		DstFootprint.Height = DstResourceDesc.Height;
		DstFootprint.Depth = 1;
		DstFootprint.RowPitch = ALIGN_UP_POW2(static_cast<u32>(DstResourceDesc.Width * BytesPerPixel), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT SrcFootprint = {};
		SrcFootprint.Footprint = DstFootprint;
		SrcFootprint.Offset = 0;

		char* PtrSrc = PtrPixelData;
		u32 SrcPitch = DstResourceDesc.Width * BytesPerPixel;
		char* PtrDst = FrameCtx.PtrUploadBuffer;
		u32 DstPitch = DstFootprint.RowPitch;

		for (u32 y = 0; y < DstResourceDesc.Height; ++y)
		{
			memcpy(PtrDst, PtrSrc, SrcPitch);
			PtrSrc += SrcPitch;
			PtrDst += DstPitch;
		}

		D3D12_TEXTURE_COPY_LOCATION SrcCopyLocation = {};
		SrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		SrcCopyLocation.pResource = FrameCtx.UploadBuffer;
		SrcCopyLocation.PlacedFootprint = SrcFootprint;

		D3D12_TEXTURE_COPY_LOCATION DstCopyLocation = {};
		DstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		DstCopyLocation.pResource = FrameCtx.BackBuffer;
		DstCopyLocation.SubresourceIndex = 0;

		FrameCtx.D3D12CommandList->CopyTextureRegion(&DstCopyLocation, 0, 0, 0, &SrcCopyLocation, nullptr);
	}

	void Present()
	{
		FrameContext& FrameCtx = GetFrameContext();

		// Transition back buffer to render target state for Dear ImGui
		D3D12_RESOURCE_BARRIER RtBarrier = TransitionBarrier(FrameCtx.BackBuffer,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
		FrameCtx.D3D12CommandList->ResourceBarrier(1, &RtBarrier);

		// Render Dear ImGui
		ImGui::Render();

		u32 RtvSize = Inst->D3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = { Inst->DescriptorHeaps.Rtv->GetCPUDescriptorHandleForHeapStart().ptr +
			Inst->SwapChain.BackBufferIndex * RtvSize };
		FrameCtx.D3D12CommandList->OMSetRenderTargets(1, &RtvHandle, FALSE, nullptr);
		FrameCtx.D3D12CommandList->SetDescriptorHeaps(1, &Inst->DescriptorHeaps.CbvSrvUav);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), FrameCtx.D3D12CommandList);

		// Transition back buffer to present state for presentation
		D3D12_RESOURCE_BARRIER PresentBarrier = TransitionBarrier(FrameCtx.BackBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		FrameCtx.D3D12CommandList->ResourceBarrier(1, &PresentBarrier);

		// Submit command list for current frame
		FrameCtx.D3D12CommandList->Close();
		ID3D12CommandList* const CommandLists[] = { FrameCtx.D3D12CommandList };
		Inst->D3D12CommandQueueDirect->ExecuteCommandLists(1, CommandLists);

		// Present
		u32 SyncInterval = Inst->bVsync ? 1 : 0;
		u32 PresentFlags = Inst->SwapChain.bSupportsTearing && !Inst->bVsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		DX_CHECK_HR(Inst->SwapChain.DXGISwapChain->Present(SyncInterval, PresentFlags));

		// Signal fence with the value for the current frame, update next available back buffer Index
		FrameCtx.FenceValue = ++Inst->Sync.FenceValue;
		DX_CHECK_HR(Inst->D3D12CommandQueueDirect->Signal(Inst->Sync.D3D12Fence, FrameCtx.FenceValue));
		Inst->SwapChain.BackBufferIndex = Inst->SwapChain.DXGISwapChain->GetCurrentBackBufferIndex();
	}

}
