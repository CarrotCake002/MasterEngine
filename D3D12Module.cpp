#include "Globals.h"
#include "D3D12Module.h"

bool D3D12Module::init()
{
	// debug functions
#if defined(_DEBUG)
	D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
	debugInterface->EnableDebugLayer();

	CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory));
#endif

	// init device
	factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
	D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));

#if defined(_DEBUG)
	if (SUCCEEDED(device.As(&infoQueue))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
	}
#else
	CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
#endif

	// command list for GPU
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&commandList));
	commandList->Reset(commandAllocator.Get(), nullptr); // resets the connection to commandAllocator and writes on previous data
	commandList->Close();

    // command queue
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&commandQueue));

	RECT rect;
	GetWindowRect(hWnd, &rect);
	UINT windowWidth = rect.right - rect.left;
	UINT windowHeight = rect.bottom - rect.top;

	// init swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = windowWidth;	// Width of the back buffer in pixels
	swapChainDesc.Height = windowHeight;// Height of the back buffer in pixels
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 32-bit RGBA format (8 bits per channel), UNORM = Unsigned normalized integer (0-255 mapped to 0.0-1.0)

	swapChainDesc.Stereo = FALSE;	// Set to TRUE for stereoscopic 3D rendering (VR/3D Vision)
	swapChainDesc.SampleDesc = { 1, 0 }; // Multisampling { Count, Quality } // Count=1: No multisampling (1 sample per pixel)
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // This buffer will be used as a render target
	swapChainDesc.BufferCount = 2;	// Double buffering:
									// - 1 front buffer (displayed)
									// - 1 back buffer (rendering)
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;	// How to scale when window size doesn't match buffer size:
													// STRETCH = Stretch the image to fit the window
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	// Modern efficient swap method:
																// - FLIP: Uses page flipping (no copying)
																// - DISCARD: Discard previous back buffer contents
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // Alpha channel behavior for window blending UNSPECIFIED = Use default behavior
	swapChainDesc.Flags = 0;	// Additional swap chain options: 0 = No special flags
								// DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH: Allow full-screen mode switches
								// DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING: Allow tearing in windowed mode (VSync off)

	ComPtr<IDXGISwapChain1> swapChain1;
	factory->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
	swapChain1.As(&swapChain);
	
	// Descriptor Heap
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptorHeapDesc.NumDescriptors = 2;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));

	// rtHandle
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	UINT rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (int i = 0; i < swapChainDesc.BufferCount; i++) {
		swapChain->GetBuffer(i, IID_PPV_ARGS(&buffers[i]));
		device->CreateRenderTargetView(buffers[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, rtvSize);
	}

	// Fence creation
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	
	/* IMGUI INIT */
	imgui = std::make_unique<ImGuiPass>(device.Get(), hWnd);
	return true;
}

void D3D12Module::update() {

}

void D3D12Module::resetCommands(void) {
	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), nullptr);
}

void D3D12Module::barrierTransitionToRenderTarget(UINT backBufferIndex) {
	CD3DX12_RESOURCE_BARRIER renderTargetBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		buffers[backBufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	commandList->ResourceBarrier(1, &renderTargetBarrier);
}

void D3D12Module::barrierTransitionToPresent(UINT backBufferIndex) {
	CD3DX12_RESOURCE_BARRIER presentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		buffers[backBufferIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
	);
	commandList->ResourceBarrier(1, &presentBarrier);
}

void D3D12Module::renderImGui(CD3DX12_CPU_DESCRIPTOR_HANDLE* rtvHandle) {
	// Set render target for imgui
	commandList->OMSetRenderTargets(1, rtvHandle, false, nullptr);

	ImGui::ShowDemoWindow();
	imgui->record(commandList.Get());
}

// USE PRE-RENDER AND POST-RENDER
void D3D12Module::render() {
	imgui->startFrame();

	// Reset command allocator and list
	resetCommands();

	/* NEW COMMANDS */
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
	UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	const FLOAT color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		backBufferIndex,
		rtvDescriptorSize
	);

	// Transition barrier
	barrierTransitionToRenderTarget(backBufferIndex);

	// Clear screen
	commandList->ClearRenderTargetView(rtvHandle, color, 0, nullptr);

	renderImGui(&rtvHandle);

	// Second Barrier
	barrierTransitionToPresent(backBufferIndex);

	// Submit commands
	commandList->Close();
	ID3D12CommandList* lists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, lists);
	swapChain->Present(1, 0);

	WaitForGPU();
}

void D3D12Module::WaitForGPU(void) {
	fenceValue++;
	commandQueue->Signal(fence.Get(), fenceValue);

	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

bool D3D12Module::cleanUp()
{
	WaitForGPU();
	CloseHandle(fenceEvent);
	return true;
}