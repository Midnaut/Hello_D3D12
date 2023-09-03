#pragma once
#include "D3D12CommonHeaders.h"

class D3D12Implementation {
	private:
		static const UINT TextureWidth = 256;
		static const UINT TextureHeight = 256;
		static const UINT TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.
		static const int32_t BufferCount = 2;

		int m_windowWidth;
		int m_windowHeight;
		float m_aspectRatio;
		HWND m_windowHandle;

		ID3D12Device8* m_mainDevice = nullptr;
		IDXGIFactory7* m_dxgiFactory = nullptr;
		
		// Pipeline objects
		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_scissorRect;
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<IDXGISwapChain3> m_swapChain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12DescriptorHeap> m_srvHeap;
		ComPtr<ID3D12PipelineState> m_pipelineState;
		ComPtr<ID3D12Resource> m_renderTargets[BufferCount];

		int m_rtvDescriptorSize = -1;

		// App resources.
		ComPtr<ID3D12Resource> m_vertexBuffer;
		ComPtr<ID3D12Resource> m_texture;
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

		// Synch objects
		UINT m_frameIndex;
		HANDLE m_fenceEvent;
		ComPtr<ID3D12Fence> m_fence;
		UINT64 m_fenceValue;
		

		void LoadPipeline();
		void LoadAssets();
		std::vector<UINT8> GenerateCheckeredTextureData();
		void PopulateCommandList();
		void WaitForPreviousFrame();

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
