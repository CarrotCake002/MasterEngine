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
	for (int i = 0; i < BUFFER_COUNT; ++i) {
		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator[i]));
	}
	device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&commandList));

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
	swapChainDesc.Width = windowWidth;
	swapChainDesc.Height = windowHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = BUFFER_COUNT;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	factory->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
	swapChain1.As(&swapChain);
	
	// Descriptor Heap
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptorHeapDesc.NumDescriptors = BUFFER_COUNT;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));

	// rtvHandle
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	UINT rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (int i = 0; i < BUFFER_COUNT; i++) {
		swapChain->GetBuffer(i, IID_PPV_ARGS(&buffers[i]));
		device->CreateRenderTargetView(buffers[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, rtvSize);
	}

	/* FENCE CREATION */
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	
	commandList->Reset(commandAllocator[0].Get(), nullptr);

	/* RESOURCE CREATION */ // PENDING TO MOVE INTO A CREATE_UPLOAD_BUFFER() FUNCTION
	UINT bufferSize = VERTEX_SIZE * sizeof(Vertex);
	CreateUploadBuffer(vertices, bufferSize);
	
	commandList->Close();

	/* IMGUI INIT */
	imgui = std::make_unique<ImGuiPass>(device.Get(), hWnd);
	ID3D12CommandList* lists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, lists);
	WaitForGPU();
	return true;
}

void D3D12Module::update() {
	bufferIndex = swapChain->GetCurrentBackBufferIndex();
	rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		bufferIndex,
		rtvSize
	);
}

void D3D12Module::resetCommands(void) {
	commandAllocator[bufferIndex]->Reset();
	commandList->Reset(commandAllocator[bufferIndex].Get(), nullptr);
}

void D3D12Module::barrierTransitionToRenderTarget() {
	CD3DX12_RESOURCE_BARRIER renderTargetBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		buffers[bufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	commandList->ResourceBarrier(1, &renderTargetBarrier);
}

void D3D12Module::barrierTransitionToPresent() {
	CD3DX12_RESOURCE_BARRIER presentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		buffers[bufferIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
	);
	commandList->ResourceBarrier(1, &presentBarrier);
}

void D3D12Module::submitCommands(void) {
	commandList->Close();
	ID3D12CommandList* lists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, lists);
	swapChain->Present(1, 0);
}

void D3D12Module::renderImGui(void) {
	// Set render target for imgui
	commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

	ImGui::ShowDemoWindow();
	imgui->record(commandList.Get());
}

void D3D12Module::clearScreen(void) {
	const FLOAT color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
}

void D3D12Module::preRender(void) {
	WaitForFence(); // SetEventOnCompletion + WaitForSingleObject
	commandAllocator[bufferIndex]->Reset();
}

void D3D12Module::postRender(void) {
	commandQueue->Signal(fence.Get(), ++currFenceValue);
	fenceValues[bufferIndex] = currFenceValue; // Store the fence for this buffer
}

void D3D12Module::render() {
	imgui->startFrame();
	resetCommands();
	barrierTransitionToRenderTarget();

	// Draw
	clearScreen();
	renderImGui();

	barrierTransitionToPresent();
	submitCommands();
	
}

void D3D12Module::WaitForGPU(void) {
	currFenceValue++;
	commandQueue->Signal(fence.Get(), currFenceValue);

	if (fence->GetCompletedValue() < currFenceValue) {
		fence->SetEventOnCompletion(currFenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

void D3D12Module::WaitForFence(void) {
	if (fence->GetCompletedValue() < fenceValues[bufferIndex]) {
		fence->SetEventOnCompletion(fenceValues[bufferIndex], fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

bool D3D12Module::cleanUp()
{
	WaitForGPU();
	CloseHandle(fenceEvent);
	return true;
}

void D3D12Module::CreateUploadBuffer(void* src, UINT size) {
	ComPtr<ID3D12Resource> gpuBuffer;
	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

	device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&gpuBuffer));

	auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&stagingBuffer));

	BYTE* pData = nullptr;
	CD3DX12_RANGE readRange(0, 0);
	stagingBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pData));

	memcpy(pData, src, size);
	stagingBuffer->Unmap(0, nullptr);

	commandList->CopyResource(gpuBuffer.Get(), stagingBuffer.Get());

	gpuBuffers.push_back(gpuBuffer);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(gpuBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	commandList->ResourceBarrier(1, &barrier);
}