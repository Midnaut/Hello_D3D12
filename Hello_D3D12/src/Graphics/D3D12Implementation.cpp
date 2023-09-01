#include "D3D12Implementation.h"


constexpr D3D_FEATURE_LEVEL min_feature_level{ D3D_FEATURE_LEVEL_11_0 };

D3D12Implementation::D3D12Implementation(HWND windowHandle, int windowWidth, int windowHeight) {
	m_windowHandle = windowHandle;
	m_windowWidth = windowWidth;
	m_windowHeight = windowHeight;
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

}


void D3D12Implementation::Shutdown() {
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
}

void D3D12Implementation::LoadPipeline() {

	// Describe and create the command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	DXCall(m_mainDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = BufferCount;
	swapChainDesc.BufferDesc.Width = m_windowWidth;
	swapChainDesc.BufferDesc.Height = m_windowHeight;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = m_windowHandle;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain> swapchain;
	DXCall(m_dxgiFactory->CreateSwapChain(m_commandQueue.Get(), &swapChainDesc, &swapchain));
	DXCall(swapchain.As(&m_swapChain));
	// No fullscreen
	DXCall(m_dxgiFactory->MakeWindowAssociation(m_windowHandle, DXGI_MWA_NO_ALT_ENTER));


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
		DXCall(m_mainDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
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


		DXCall(D3DCompileFromFile(shaderFilePath.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &vertexShaderCompileErrors));
		DXCall(D3DCompileFromFile(shaderFilePath.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &pixelShaderCompileErrors));
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
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = {reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize()};
		psoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };

	}
}

void D3D12Implementation::PopulateCommandList() {

}

void D3D12Implementation::WaitForPreviousFrame() {

}