#pragma once
#include "D3D12CommonHeaders.h"

class D3D12Implementation {
	private:
		static const int32_t BufferCount = 2;
		int m_windowWidth;
		int m_windowHeight;
		HWND m_windowHandle;

		ID3D12Device8* m_mainDevice = nullptr;
		IDXGIFactory7* m_dxgiFactory = nullptr;

		void LoadPipeline();
		void LoadAssets();
		void PopulateCommandList();
		void WaitForPreviousFrame();

		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<IDXGISwapChain> m_swapChain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12Resource> m_renderTargets[BufferCount];
		int m_rtvDescriptorSize = -1;

	public:
		D3D12Implementation(HWND windowHandle, int windowWidth, int windowHeight);
		~D3D12Implementation();
		bool Initialize();
		void Shutdown();
		void Render();
};

template<typename T>
constexpr void release(T& resource)
{
	if (resource)
	{
		resource->Release();
		resource = nullptr;
	}
};
