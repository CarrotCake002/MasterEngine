#pragma once

#include "Module.h"
#include "ImGuiPass.h"

class D3D12Module : public Module
{
public:
	D3D12Module(HWND hWnd) : hWnd(hWnd) {};
	~D3D12Module() = default;

	bool init();
	void update();
	void render();
	bool cleanUp();

	// Commands
	void resetCommands(void);
	void submitCommands(void);

	// Transition barrier
	void barrierTransitionToRenderTarget(UINT backBufferIndex);
	void barrierTransitionToPresent(UINT backBufferIndex);

	// Screen
	void clearScreen(const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle);

	// ImGui
	void renderImGui(CD3DX12_CPU_DESCRIPTOR_HANDLE *rtvHandle);

	void WaitForGPU(void);
private:
	// device handling
	ComPtr<IDXGIFactory7> factory;
	ComPtr<IDXGIAdapter4> adapter;
	ComPtr<ID3D12Device5> device;
	ComPtr<ID3D12InfoQueue> infoQueue;

	// swapChain command handling
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList1> commandList;
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<IDXGISwapChain4> swapChain;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	
	// Fence
	ComPtr<ID3D12Fence> fence;
	UINT64 fenceValue;
	HANDLE fenceEvent;

	// ImGUI
	std::unique_ptr <ImGuiPass> imgui;

	HWND hWnd;

	ComPtr<ID3D12Resource> buffers[2];

#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugInterface;
#endif
};

