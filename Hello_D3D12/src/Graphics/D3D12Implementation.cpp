#include "D3D12Implementation.h"


constexpr D3D_FEATURE_LEVEL min_feature_level{ D3D_FEATURE_LEVEL_11_0 };

D3D12Implementation::D3D12Implementation(HWND windowHandle, int windowWidth, int windowHeight) {
	m_windowHandle = windowHandle;
	m_windowWidth = windowWidth;
	m_windowHeight = windowHeight;
	m_frameIndex = 0;

	m_viewport = {};
	m_viewport.TopLeftX = 0.0;
	m_viewport.TopLeftY = 0.0;
	m_viewport.Width = static_cast<float>(windowWidth);
	m_viewport.Height = static_cast<float>(windowHeight);
	m_viewport.MinDepth = D3D12_MIN_DEPTH;
	m_viewport.MaxDepth = D3D12_MAX_DEPTH;
	
	m_scissorRect = {};
	m_scissorRect.left = 0;
	m_scissorRect.top = 0;
	m_scissorRect.right = static_cast<LONG>(windowWidth);
	m_scissorRect.bottom = static_cast<LONG>(windowHeight);

	m_rtvDescriptorSize = 0;

	m_aspectRatio = m_viewport.Width / m_viewport.Height;

	// Shut the warnings up
	m_fenceEvent = nullptr;
	m_fenceValue = 0;
	m_vertexBufferView = {};

	spdlog::info("D3D12Implementation Constructor Called");
}

D3D12Implementation::~D3D12Implementation() {

	spdlog::info("D3D12Implementation Destructor Called");
}

bool D3D12Implementation::Initialize() {

	
	if (m_mainDevice) Shutdown();
	UINT32 dxgi_factory_flags{ 0 };
#ifdef _DEBUG
	// Enable debug layer
	// NOTE: Requires "Graphics Tools" optional feature
	{
		ComPtr<ID3D12Debug> debug_interface;
		DXCall(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
		debug_interface->EnableDebugLayer();
		dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif // _DEBUG

	HRESULT hr{ S_OK };
	DXCall( hr = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_dxgiFactory)));
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}


	// Determine which adapter (graphics card) to use
	ComPtr<IDXGIAdapter4> main_adapter;
	{
		IDXGIAdapter4* adapter{ nullptr };

		// Get adapters in descending order of performance
		for (UINT32 i{ 0 };
			m_dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
			++i)
		{
			// Pick the first adapter that supports the minimum feature level.
			if (SUCCEEDED(D3D12CreateDevice(adapter, min_feature_level, __uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
			release(adapter);
		}


		if (!adapter) {
			Shutdown();
			return false;
		}
		
		main_adapter.Attach(adapter);
	}

	// Determine what is the max feature level
	D3D_FEATURE_LEVEL max_feature_level;
	{
		constexpr D3D_FEATURE_LEVEL feature_levels[4]{
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_12_1,
		};

		D3D12_FEATURE_DATA_FEATURE_LEVELS feature_level_info{};
		feature_level_info.NumFeatureLevels = _countof(feature_levels);
		feature_level_info.pFeatureLevelsRequested = feature_levels;

		ComPtr<ID3D12Device> device;
		DXCall(D3D12CreateDevice(main_adapter.Get(), min_feature_level, IID_PPV_ARGS(&device)));
		DXCall(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &feature_level_info, sizeof(feature_level_info)));
		max_feature_level = feature_level_info.MaxSupportedFeatureLevel;

		assert(max_feature_level >= min_feature_level);
		if (max_feature_level < min_feature_level) {
			Shutdown();
			return false;
		}
	}

	// Create a ID3D12Device (virtual adapter)
	DXCall(hr = D3D12CreateDevice(main_adapter.Get(), max_feature_level, IID_PPV_ARGS(&m_mainDevice)));
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	NAME_D3D12_OBJECT(m_mainDevice, L"Main D3D12 Device");

#ifdef _DEBUG
	{
		ComPtr<ID3D12InfoQueue> info_queue;
		DXCall(m_mainDevice->QueryInterface(IID_PPV_ARGS(&info_queue)));

		info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
	}
#endif // _DEBUG

	// Load Pipeline
	LoadPipeline();
	// Load Assets
	LoadAssets();

	return true;
}

void D3D12Implementation::Render() {

	// Record all commands we need to render into the command list
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommanLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommanLists), ppCommanLists);

	// Present the frame
	DXCall(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void D3D12Implementation::Shutdown() {
	
	WaitForPreviousFrame();

	release(m_dxgiFactory);

#ifdef _DEBUG
	{
		ComPtr<ID3D12InfoQueue> info_queue;
		DXCall(m_mainDevice->QueryInterface(IID_PPV_ARGS(&info_queue)));

		info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
		info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
		info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);

		ComPtr<ID3D12DebugDevice> debug_device;
		DXCall(m_mainDevice->QueryInterface(IID_PPV_ARGS(&debug_device)));
		release(m_mainDevice);

		DXCall(debug_device->ReportLiveDeviceObjects( 
			D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL 
		));
	}
#endif // _DEBUG

	release(m_mainDevice);


	CloseHandle(m_fenceEvent);
}

void D3D12Implementation::LoadPipeline() {

	// Describe and create the command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	DXCall(m_mainDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = BufferCount;
	swapChainDesc.Width = m_windowWidth;
	swapChainDesc.Height = m_windowHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapchain;
	DXCall(m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(),m_windowHandle, &swapChainDesc, nullptr, nullptr, &swapchain));
	
	DXCall(swapchain.As(&m_swapChain));
	// No fullscreen
	DXCall(m_dxgiFactory->MakeWindowAssociation(m_windowHandle, DXGI_MWA_NO_ALT_ENTER));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create Descriptor Heaps
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = BufferCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DXCall(m_mainDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_mainDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create Frame Resources 
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
		rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

		// Create a RTV for each frame
		for (UINT32 i{ 0 }; i < BufferCount; i++)
		{
			DXCall(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
			m_mainDevice->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += m_rtvDescriptorSize;
		}

		DXCall(m_mainDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
	}
}

void D3D12Implementation::LoadAssets() {

	// Create an empty root signature
	{
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.NumParameters = 0;
		rootSignatureDesc.pParameters = nullptr;
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.pStaticSamplers = nullptr;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		DXCall(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		DXCall(m_mainDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
			IID_PPV_ARGS(&m_rootSignature)));
	}

	// Create the pipeline state
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> vertexShaderCompileErrors;
		ComPtr<ID3DBlob> pixelShader;
		ComPtr<ID3DBlob> pixelShaderCompileErrors;

#ifdef _DEBUG
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif //_Debug

		// generate relative asset path
		WCHAR assetsPath[512];
		GetModuleFileName(nullptr, assetsPath, sizeof(assetsPath));

		WCHAR* lastSlash = wcsrchr(assetsPath, L'\\');
		if (lastSlash)
		{
			*(lastSlash + 1) = L'\0';
		}


		std::wstring shaderFile = L"Shaders\\shaders.hlsl";
		std::wstring shaderFilePath = assetsPath + shaderFile;


		DXCall(D3DCompileFromFile(shaderFilePath.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags,
			0, &vertexShader, &vertexShaderCompileErrors));
		DXCall(D3DCompileFromFile(shaderFilePath.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags,
			0, &pixelShader, &pixelShaderCompileErrors));
		// Define the Vertex Input Layout

		//	D3D12_INPUT_ELEMENT_DESC
		//  LPCSTR                     SemanticName;
		//  UINT                       SemanticIndex;
		//  DXGI_FORMAT                Format;
		//  UINT                       InputSlot;
		//  UINT                       AlignedByteOffset;
		//  D3D12_INPUT_CLASSIFICATION InputSlotClass;
		//  UINT                       InstanceDataStepRate;

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		};

		// Describe and create the graphics pipeline state object (PSO)

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		
		D3D12_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerDesc.FrontCounterClockwise = FALSE;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.DepthClipEnable = TRUE;
		rasterizerDesc.MultisampleEnable = FALSE;
		rasterizerDesc.AntialiasedLineEnable = FALSE;
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
		renderTargetBlendDesc.BlendEnable = FALSE;
		renderTargetBlendDesc.LogicOpEnable = FALSE;
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
		renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
		renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			blendDesc.RenderTarget[i] = renderTargetBlendDesc;
		}

		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = {reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize()};
		psoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
		psoDesc.RasterizerState = rasterizerDesc;
		psoDesc.BlendState = blendDesc;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		DXCall(m_mainDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	// Create the command list;
	DXCall(m_mainDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), 
		m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
	// Command lists are created in the recording state.Closing now, as nothing to record.
	DXCall(m_commandList->Close());

	// Create the vertex buffer
	{
		struct Vertex
		{
			glm::vec3 position;
			glm::vec4 color;
		};

		Vertex triangleVerts[] =
		{
			{ glm::vec3(0.0f,	 0.25f * m_aspectRatio,	0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ glm::vec3(0.25f,	-0.25f * m_aspectRatio, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
			{ glm::vec3(-0.25f, -0.25f * m_aspectRatio, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
		};

		const UINT vertexBufferSize = sizeof(triangleVerts);

		// Create and upload the vertex information
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		DXGI_SAMPLE_DESC sampleDesc = {};
		sampleDesc.Count = 1;
		sampleDesc.Quality = 0;

		D3D12_RESOURCE_DESC vertexBufferDesc = {};
		vertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		vertexBufferDesc.Alignment = 0;
		vertexBufferDesc.Width = vertexBufferSize;
		vertexBufferDesc.Height = 1,
		vertexBufferDesc.DepthOrArraySize = 1;
		vertexBufferDesc.MipLevels = 1;
		vertexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		vertexBufferDesc.SampleDesc = sampleDesc;
		vertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		vertexBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		DXCall(m_mainDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc, 
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffer)));

		// Copy the triangle data into the vertex buffer
		UINT8* pVertexDataBegin;
		// We do not intend to read this resource on CPU
		D3D12_RANGE readRange = {};
		readRange.Begin = 0;
		readRange.End = 0;

		DXCall( m_vertexBuffer->Map( 0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin) ) );
		memcpy(pVertexDataBegin, triangleVerts, vertexBufferSize);
		m_vertexBuffer->Unmap(0, nullptr);

		// Init vertex buffer view
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	// Create synch objects and wait till assets have been uploaded
	{
		DXCall(m_mainDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			DXCall(HRESULT_FROM_WIN32(GetLastError()));
		}

		// wait for the command list to execute. Wait for setup to complete before continuing.
		WaitForPreviousFrame();
	}

}

void D3D12Implementation::PopulateCommandList() {

	// Reset command allocator for this frame
	DXCall(m_commandAllocator->Reset());

	// Reset the command list
	DXCall(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Set state
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// set back buffer for render taget
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	m_commandList->ResourceBarrier(1, &barrier);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
	rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += (static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record the commands
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// Inidcate the backbuffer
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	m_commandList->ResourceBarrier(1, &barrier);

	// Close recording
	DXCall(m_commandList->Close());
}

void D3D12Implementation::WaitForPreviousFrame() {

	// signial and increment fence value;
	const UINT64 localFenceValue = m_fenceValue;
	DXCall(m_commandQueue->Signal(m_fence.Get(), localFenceValue));
	m_fenceValue++;


	// wait until the previous frame is finished 
	if (m_fence->GetCompletedValue() < localFenceValue)
	{
		DXCall(m_fence->SetEventOnCompletion(localFenceValue, m_fenceEvent));
		// No timeout
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

}