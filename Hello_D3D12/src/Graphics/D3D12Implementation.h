#pragma once
#include "D3D12CommonHeaders.h"

class D3D12Implementation {
	private:
		static const UINT TextureWidth = 256;
		static const UINT TextureHeight = 256;
		static const UINT TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.
		static const int32_t BufferCount = 2;

		struct Node
		{
			glm::vec4 offset;
		};

		struct SceneConstantBuffer
		{
			// This is here because the const buffer register will have this int take up the space of 4, so we need
			// to pad the struct to eequal the sie of the shader cont buffer
			int nodeIdx;
			int padding[3];

			Node nodes[2];
		};
		//static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

		int m_windowWidth;
		int m_windowHeight;
		int m_frameCounter;
		float m_aspectRatio;
		HWND m_windowHandle;

		ID3D12Device8* m_mainDevice = nullptr;
		IDXGIFactory7* m_dxgiFactory = nullptr;
		
		// Pipeline objects
		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_scissorRect;
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12CommandAllocator> m_bundleAllocator;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12GraphicsCommandList> m_bundleCommandList;
		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<IDXGISwapChain3> m_swapChain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12DescriptorHeap> m_srvCbvHeap;
		ComPtr<ID3D12PipelineState> m_pipelineState;
		ComPtr<ID3D12Resource> m_renderTargets[BufferCount];

		int m_srvCbvDescriptorSize = -1;
		int m_rtvDescriptorSize = -1;

		// App resources.
		ComPtr<ID3D12Resource> m_vertexBuffer;
		ComPtr<ID3D12Resource> m_texture;
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

		ComPtr<ID3D12Resource> m_constantBuffer;
		SceneConstantBuffer m_constantBufferData;
		UINT8* m_pCbvDataBegin;

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
		void Update();
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
