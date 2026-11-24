#pragma once

#include "Module.h"
#include "ImGuiPass.h"

#define BUFFER_COUNT 2
#define VERTEX_SIZE 3

struct Vertex {
	float x, y, z;
};

class D3D12Module : public Module
{
public:
	D3D12Module(HWND hWnd) : hWnd(hWnd) {};
	~D3D12Module() = default;

	bool init(void);
	void update(void);
	bool cleanUp(void);

	// Commands
	void resetCommands(void);
	void submitCommands(void);

	// Transition barrier
	void barrierTransitionToRenderTarget(void);
	void barrierTransitionToPresent(void);

	// Screen
	void clearScreen(void);

	// render
	void render(void);
	void preRender(void);
	void postRender(void);
	void renderImGui(void);

	// Sync
	void WaitForFence(void);
	void WaitForGPU(void);

	// Buffers
	void CreateUploadBuffer(void* src, UINT size);

	// Getters
	void GetCommandQueue(ComPtr<ID3D12CommandQueue> commandQueue) const { commandQueue = this->commandQueue; };
	void GetCommandList(ComPtr<ID3D12GraphicsCommandList1> commandList) const { commandList = this->commandList; };

private:
	// Device
	ComPtr<IDXGIFactory7> factory;
	ComPtr<IDXGIAdapter4> adapter;
	ComPtr<ID3D12Device5> device;
	ComPtr<ID3D12InfoQueue> infoQueue;

	// Commands
	ComPtr<ID3D12CommandAllocator> commandAllocator[BUFFER_COUNT];
	ComPtr<ID3D12GraphicsCommandList1> commandList;
	ComPtr<ID3D12CommandQueue> commandQueue;

	// Buffers
	ComPtr<IDXGISwapChain4> swapChain;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	ComPtr<ID3D12Resource> buffers[BUFFER_COUNT];
	ComPtr<ID3D12Resource> vertexBuffer = nullptr;
	ComPtr<ID3D12Resource> stagingBuffer = nullptr;
	std::vector<ComPtr<ID3D12Resource>> gpuBuffers;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	UINT bufferIndex;
	UINT rtvSize;

	// Fence
	ComPtr<ID3D12Fence> fence;
	UINT64 fenceValues[BUFFER_COUNT];
	UINT64 currFenceValue;
	HANDLE fenceEvent;

	// ImGUI
	std::unique_ptr <ImGuiPass> imgui;

	HWND hWnd;

	Vertex vertices[VERTEX_SIZE] = {
		{ 0.0f, 0.5f, 0.0f },
		{ 0.5f, -0.5f, 0.0f },
		{ -0.5f, -0.5f, 0.0f },
	};

#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugInterface;
#endif
};

