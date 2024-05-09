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

static inline const char* GetHRMessage(HRESULT hr)
{
	char* msg = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&msg, 0, NULL);
	return msg;
}

static inline void DX_CHECK_HR(HRESULT hr)
{
	if (FAILED(hr))
		FATAL_ERROR("DX12Backend", GetHRMessage(hr));
}

namespace DX12Backend
{

	static constexpr uint32_t SWAP_CHAIN_BACK_BUFFER_COUNT = 2u;

	struct FrameContext
	{
		ID3D12CommandAllocator* d3d12CommandAllocator = nullptr;
		ID3D12GraphicsCommandList6* d3d12CommandList = nullptr;

		ID3D12Resource* backBuffer = nullptr;
		ID3D12Resource* uploadBuffer = nullptr;
		char* uploadBufferPtr = nullptr;

		uint64_t fenceValue = 0ull;
	};

	struct Instance
	{
		IDXGIAdapter4* dxgiAdapter = nullptr;
		ID3D12Device8* d3d12Device = nullptr;
		ID3D12CommandQueue* d3d12CommandQueueDirect = nullptr;

		bool vsync = false;

		FrameContext frameContext[SWAP_CHAIN_BACK_BUFFER_COUNT];

		struct SwapChain
		{
			IDXGISwapChain4* dxgiSwapChain = nullptr;
			uint32_t backBufferIndex = 0u;
			bool supportsTearing = false;

			uint32_t outputWidth = 0;
			uint32_t outputHeight = 0;
		} swapChain;

		struct Sync
		{
			ID3D12Fence* d3d12Fence = nullptr;
			HANDLE fenceEventHandle = HANDLE();
			uint64_t fenceValue = 0ull;
		} sync;

		struct DescriptorHeaps
		{
			ID3D12DescriptorHeap* rtv = nullptr;
			ID3D12DescriptorHeap* cbvSrvUav = nullptr;
		} descriptorHeaps;
	} static *inst;

	static inline FrameContext& GetFrameContext()
	{
		return inst->frameContext[inst->swapChain.backBufferIndex];
	}
	
	static D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
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

	void Init()
	{
		LOG_INFO("DX12Backend", "Init");

		inst = new Instance();

		// Enable debug layer
#ifdef _DEBUG
		ID3D12Debug* debugInterface = nullptr;
		DX_CHECK_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
		debugInterface->EnableDebugLayer();

		// Enable GPU based validation
#ifdef DX12_GPU_BASED_VALIDATION
		ID3D12Debug1* debugInterface1 = nullptr;
		DXCheckHR(debugInterface->QueryInterface(IID_PPV_ARGS(&debugInterface1)));
		debugInterface1->SetEnableGPUBasedValidation(true);
#endif
#endif

		uint32_t factoryCreateFlags = 0;
#ifdef _DEBUG
		factoryCreateFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		// Create factory
		IDXGIFactory7* dxgiFactory7 = nullptr;
		DX_CHECK_HR(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory7)));

		// Select adapter
		D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_12_0;
		IDXGIAdapter1* dxgiAdapter1 = nullptr;
		uint64_t maxDedicatedVideoMemory = 0;

		for (uint32_t adapterIndex = 0; dxgiFactory7->EnumAdapters1(adapterIndex, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 adapterDesc = {};
			DX_CHECK_HR(dxgiAdapter1->GetDesc1(&adapterDesc));

			if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1, minFeatureLevel, __uuidof(ID3D12Device), nullptr)) &&
				adapterDesc.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = adapterDesc.DedicatedVideoMemory;
				DX_CHECK_HR(dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&inst->dxgiAdapter)));
			}
		}

		// Create D3D12 device
		DX_CHECK_HR(D3D12CreateDevice(inst->dxgiAdapter, minFeatureLevel, IID_PPV_ARGS(&inst->d3d12Device)));

		// Set info queue behavior and filters
		ID3D12InfoQueue* d3d12InfoQueue = nullptr;
		DX_CHECK_HR(inst->d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));
		DX_CHECK_HR(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
		DX_CHECK_HR(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
		DX_CHECK_HR(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));

		// Check for tearing support
		BOOL allowTearing = FALSE;
		DX_CHECK_HR(dxgiFactory7->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(BOOL)));
		inst->swapChain.supportsTearing = (allowTearing == TRUE);

		// Create swap chain command queue
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;
		DX_CHECK_HR(inst->d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&inst->d3d12CommandQueueDirect)));

		// Create swap chain
		HWND hwnd = GetActiveWindow();

		RECT windowRect = {};
		GetWindowRect(hwnd, &windowRect);

		inst->swapChain.outputWidth = windowRect.right - windowRect.left;
		inst->swapChain.outputHeight = windowRect.bottom - windowRect.top;

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = inst->swapChain.outputWidth;
		swapChainDesc.Height = inst->swapChain.outputHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.Stereo = FALSE;
		swapChainDesc.SampleDesc = { 1, 0 };
		swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		swapChainDesc.BufferCount = SWAP_CHAIN_BACK_BUFFER_COUNT;
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapChainDesc.Flags = inst->swapChain.supportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		IDXGISwapChain1* dxgiSwapChain1 = nullptr;
		DX_CHECK_HR(dxgiFactory7->CreateSwapChainForHwnd(inst->d3d12CommandQueueDirect, hwnd, &swapChainDesc, nullptr, nullptr, &dxgiSwapChain1));
		DX_CHECK_HR(dxgiSwapChain1->QueryInterface(&inst->swapChain.dxgiSwapChain));
		DX_CHECK_HR(dxgiFactory7->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		inst->swapChain.backBufferIndex = inst->swapChain.dxgiSwapChain->GetCurrentBackBufferIndex();

		// Create command allocators and command lists
		for (uint32_t i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			DX_CHECK_HR(inst->d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&inst->frameContext[i].d3d12CommandAllocator)));
			DX_CHECK_HR(inst->d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				inst->frameContext[i].d3d12CommandAllocator, nullptr, IID_PPV_ARGS(&inst->frameContext[i].d3d12CommandList)));
		}

		// Create fence and fence event
		DX_CHECK_HR(inst->d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&inst->sync.d3d12Fence)));
		inst->sync.fenceEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (!inst->sync.fenceEventHandle)
			FATAL_ERROR("DX12Backend", "Failed to create fence event handle");

		// Create upload buffer
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		// Need to make sure that the upload buffer matches the back buffer it will copy to
		resourceDesc.Width = AlignUp(inst->swapChain.outputWidth * 4, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT) * inst->swapChain.outputHeight;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		for (uint32_t i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			DX_CHECK_HR(inst->d3d12Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
				&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&inst->frameContext[i].uploadBuffer)));
			inst->frameContext[i].uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&inst->frameContext[i].uploadBufferPtr));
		}

		// Create descriptor heaps
		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
		descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		descriptorHeapDesc.NumDescriptors = SWAP_CHAIN_BACK_BUFFER_COUNT;
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		descriptorHeapDesc.NodeMask = 0;
		DX_CHECK_HR(inst->d3d12Device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&inst->descriptorHeaps.rtv)));

		descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptorHeapDesc.NumDescriptors = 1;
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DX_CHECK_HR(inst->d3d12Device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&inst->descriptorHeaps.cbvSrvUav)));

		// Create render target views for all back buffers
		for (uint32_t i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			inst->swapChain.dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&inst->frameContext[i].backBuffer));

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = swapChainDesc.Format;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;
			rtvDesc.Texture2D.PlaneSlice = 0;

			uint32_t rtvSize = inst->d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			inst->d3d12Device->CreateRenderTargetView(inst->frameContext[i].backBuffer, &rtvDesc,
				{ inst->descriptorHeaps.rtv->GetCPUDescriptorHandleForHeapStart().ptr + i * rtvSize });
		}

		// Initialize Dear ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(inst->d3d12Device, SWAP_CHAIN_BACK_BUFFER_COUNT, swapChainDesc.Format, inst->descriptorHeaps.cbvSrvUav,
			inst->descriptorHeaps.cbvSrvUav->GetCPUDescriptorHandleForHeapStart(), inst->descriptorHeaps.cbvSrvUav->GetGPUDescriptorHandleForHeapStart());
	}

	void Exit()
	{
		LOG_INFO("DX12Backend", "Exit");

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		for (uint32_t i = 0; i < SWAP_CHAIN_BACK_BUFFER_COUNT; ++i)
		{
			inst->frameContext[i].uploadBuffer->Unmap(0, nullptr);
		}
		CloseHandle(inst->sync.fenceEventHandle);

		delete inst;
	}

	void BeginFrame()
	{
		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX12_NewFrame();
		ImGui::NewFrame();

		FrameContext& frameContext = GetFrameContext();

		// Wait for in-flight frame for the current back buffer
		if (inst->sync.d3d12Fence->GetCompletedValue() < frameContext.fenceValue)
		{
			DX_CHECK_HR(inst->sync.d3d12Fence->SetEventOnCompletion(frameContext.fenceValue, inst->sync.fenceEventHandle));
			WaitForSingleObjectEx(inst->sync.fenceEventHandle, UINT32_MAX, FALSE);
		}

		// Reset command allocator and command list for the next back buffer
		if (frameContext.fenceValue > 0)
		{
			DX_CHECK_HR(frameContext.d3d12CommandAllocator->Reset());
			DX_CHECK_HR(frameContext.d3d12CommandList->Reset(frameContext.d3d12CommandAllocator, nullptr));
		}
	}

	void EndFrame()
	{
	}

	void CopyToBackBuffer(char* pixelData, uint32_t numBytes)
	{
		FrameContext& frameContext = GetFrameContext();

		D3D12_RESOURCE_BARRIER copyDstBarrier = TransitionBarrier(frameContext.backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
		frameContext.d3d12CommandList->ResourceBarrier(1, &copyDstBarrier);

		uint32_t bpp = 4;
		D3D12_RESOURCE_DESC dstDesc = frameContext.backBuffer->GetDesc();

		D3D12_SUBRESOURCE_FOOTPRINT dstFootprint = {};
		dstFootprint.Format = dstDesc.Format;
		dstFootprint.Width = dstDesc.Width;
		dstFootprint.Height = dstDesc.Height;
		dstFootprint.Depth = 1;
		dstFootprint.RowPitch = AlignUp<uint32_t>(dstDesc.Width * bpp, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT srcFootprint = {};
		srcFootprint.Footprint = dstFootprint;
		srcFootprint.Offset = 0;

		char* srcPtr = pixelData;
		uint32_t srcPitch = dstDesc.Width * bpp;
		char* dstPtr = frameContext.uploadBufferPtr;
		uint32_t dstPitch = dstFootprint.RowPitch;

		for (uint32_t y = 0; y < dstDesc.Height; ++y)
		{
			memcpy(dstPtr, srcPtr, srcPitch);
			srcPtr += srcPitch;
			dstPtr += dstPitch;
		}

		D3D12_TEXTURE_COPY_LOCATION srcCopyLocation = {};
		srcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcCopyLocation.pResource = frameContext.uploadBuffer;
		srcCopyLocation.PlacedFootprint = srcFootprint;

		D3D12_TEXTURE_COPY_LOCATION dstCopyLocation = {};
		dstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstCopyLocation.pResource = frameContext.backBuffer;
		dstCopyLocation.SubresourceIndex = 0;

		frameContext.d3d12CommandList->CopyTextureRegion(&dstCopyLocation, 0, 0, 0, &srcCopyLocation, nullptr);
	}

	void Present()
	{
		FrameContext& frameContext = GetFrameContext();

		// Transition back buffer to render target state for Dear ImGui
		D3D12_RESOURCE_BARRIER renderTargetBarrier = TransitionBarrier(frameContext.backBuffer,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
		frameContext.d3d12CommandList->ResourceBarrier(1, &renderTargetBarrier);

		// Render Dear ImGui
		ImGui::Render();

		uint32_t rtvSize = inst->d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = { inst->descriptorHeaps.rtv->GetCPUDescriptorHandleForHeapStart().ptr +
			inst->swapChain.backBufferIndex * rtvSize };
		frameContext.d3d12CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
		frameContext.d3d12CommandList->SetDescriptorHeaps(1, &inst->descriptorHeaps.cbvSrvUav);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), frameContext.d3d12CommandList);

		// Transition back buffer to present state for presentation
		D3D12_RESOURCE_BARRIER presentBarrier = TransitionBarrier(frameContext.backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		frameContext.d3d12CommandList->ResourceBarrier(1, &presentBarrier);

		// Submit command list for current frame
		frameContext.d3d12CommandList->Close();
		ID3D12CommandList* const commandLists[] = { frameContext.d3d12CommandList };
		inst->d3d12CommandQueueDirect->ExecuteCommandLists(1, commandLists);

		// Present
		uint32_t syncInterval = inst->vsync ? 1 : 0;
		uint32_t presentFlags = inst->swapChain.supportsTearing && !inst->vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		DX_CHECK_HR(inst->swapChain.dxgiSwapChain->Present(syncInterval, presentFlags));

		// Wait for next back buffer to finish presenting
		frameContext.fenceValue = ++inst->sync.fenceValue;
		DX_CHECK_HR(inst->d3d12CommandQueueDirect->Signal(inst->sync.d3d12Fence, frameContext.fenceValue));
		inst->swapChain.backBufferIndex = inst->swapChain.dxgiSwapChain->GetCurrentBackBufferIndex();
	}

}
