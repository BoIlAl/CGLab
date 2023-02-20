#include "renderer.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <DirectXMath.h>

#include <cmath>
#include <chrono>

#include "shaderCompiler.h"
#include "common.h"
#include "toneMapping.h"
#include "Camera.h"

static constexpr UINT MaxLightNum = 3;


struct Vertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;
	DirectX::XMFLOAT3 normal;
};


struct ConstantBuffer
{
	DirectX::XMFLOAT4X4 modelMatrix;
	DirectX::XMFLOAT4X4 vpMatrix;
};

struct LightBuffer
{
	DirectX::XMUINT4 lightsCount; // r
	PointLight lights[MaxLightNum];
};



Renderer* Renderer::CreateRenderer(HWND hWnd)
{
	Renderer* pRenderer = new Renderer();

	if (pRenderer->Init(hWnd))
	{
		return pRenderer;
	}

	delete pRenderer;
	return nullptr;
}

void Renderer::DeleterRenderer(Renderer*& pRenderer)
{
	if (pRenderer != nullptr)
	{
		pRenderer->Release();
		delete pRenderer;
		pRenderer = nullptr;
	}
}

Renderer::Renderer()
	: m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_pSwapChain(nullptr)
	, m_pBackBufferRTV(nullptr)
	, m_pDepthTexture(nullptr)
	, m_pDepthTextureDSV(nullptr)
	, m_pHDRRenderTarget(nullptr)
	, m_pHDRTextureRTV(nullptr)
	, m_pHDRTextureSRV(nullptr)
	, m_pRasterizerState(nullptr)
	, m_pDepthStencilState(nullptr)
	, m_pLightBuffer(nullptr)
	, m_pCubeVertexBuffer(nullptr)
	, m_pCubeIndexBuffer(nullptr)
	, m_cubeIndexCount(0)
	, m_pPlaneVertexBuffer(nullptr)
	, m_pPlaneIndexBuffer(nullptr)
	, m_planeIndexCount(0)
	, m_pCubeConstantBuffer(nullptr)
	, m_pPlaneConstantBuffer(nullptr)
	, m_pVertexShader(nullptr)
	, m_pPixelShader(nullptr)
	, m_pInputLayout(nullptr)
	, m_windowWidth(0)
	, m_windowHeight(0)
	, m_pShaderCompiler(nullptr)
	, m_pAnnotation(nullptr)
	, m_startTime(0)
	, m_currentTime(0)
	, m_timeFromLastFrame(0)
	, m_pCamera(nullptr)
#ifdef _DEBUG
	, m_isDebug(true)
#else
	, m_isDebug(false)
#endif
	, m_pToneMapping(nullptr)
{}

Renderer::~Renderer()
{}


bool Renderer::Init(HWND hWnd)
{
	IDXGIFactory* pFactory = nullptr;
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&pFactory));
	
	if (SUCCEEDED(hr))
	{
		hr = CreateDevice(pFactory);
	}

	if (SUCCEEDED(hr))
	{
		hr = CreateSwapChain(pFactory, hWnd);
	}

	if (SUCCEEDED(hr))
	{
		hr = CreateBackBuffer();
	}

	if (SUCCEEDED(hr))
	{
		hr = CreatePipelineStateObjects();
	}

	if (SUCCEEDED(hr))
	{
		hr = CreateSceneResources();
	}

	bool res = SUCCEEDED(hr);

	if (SUCCEEDED(hr))
	{
		m_pCamera = new Camera();
		if (!m_pCamera) {
			res = false;
		}
	}
	
	if (!res)
	{
		Release();
	}

	SafeRelease(pFactory);

	return SUCCEEDED(hr);
}


void Renderer::Release()
{
	SafeRelease(m_pInputLayout);
	SafeRelease(m_pPixelShader);
	SafeRelease(m_pVertexShader);
	SafeRelease(m_pLightBuffer);
	SafeRelease(m_pCubeIndexBuffer);
	SafeRelease(m_pCubeVertexBuffer);
	SafeRelease(m_pPlaneIndexBuffer);
	SafeRelease(m_pPlaneVertexBuffer);
	SafeRelease(m_pCubeConstantBuffer);
	SafeRelease(m_pPlaneConstantBuffer);
	SafeRelease(m_pDepthStencilState);
	SafeRelease(m_pRasterizerState);
	SafeRelease(m_pHDRTextureSRV);
	SafeRelease(m_pHDRTextureRTV);
	SafeRelease(m_pHDRRenderTarget);
	SafeRelease(m_pDepthTextureDSV);
	SafeRelease(m_pDepthTexture);
	SafeRelease(m_pBackBufferRTV);
	SafeRelease(m_pSwapChain);
	SafeRelease(m_pContext);
	SafeRelease(m_pAnnotation);

	delete m_pShaderCompiler;
	delete m_pToneMapping;

	if (m_pCamera != nullptr)
	{
		delete m_pCamera;
	}

	if (m_isDebug) 
	{
		ID3D11Debug* pDebug = nullptr;
		HRESULT hr = m_pDevice->QueryInterface(IID_PPV_ARGS(&pDebug));
		if (SUCCEEDED(hr))
		{
			UINT references = m_pDevice->Release();
			if (references > 1) 
			{
				pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
			}
			SafeRelease(pDebug);
		}
	}

}


HRESULT Renderer::CreateDevice(IDXGIFactory* pFactory)
{
	HRESULT hr = S_OK;

	IDXGIAdapter* pAdapter = nullptr;
	UINT adapterIdx = 0;

	while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter)))
	{
		DXGI_ADAPTER_DESC adapterDesc = {};
		pAdapter->GetDesc(&adapterDesc);

		if (wcscmp(adapterDesc.Description, L"Microsoft Basic Render Driver") != 0)
		{
			break;
		}

		pAdapter->Release();
		++adapterIdx;
	}

	if (pAdapter == nullptr)
	{
		hr = E_FAIL;
	}

	if (SUCCEEDED(hr))
	{
		D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

		hr = D3D11CreateDevice(
			pAdapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			nullptr,
			D3D11_CREATE_DEVICE_DEBUG,
			levels,
			1,
			D3D11_SDK_VERSION,
			&m_pDevice,
			nullptr,
			&m_pContext
		);
	}

	if (SUCCEEDED(hr))
	{
		hr = m_pContext->QueryInterface(IID_PPV_ARGS(&m_pAnnotation));
	}

	SafeRelease(pAdapter);

	return hr;
}

HRESULT Renderer::CreateSwapChain(IDXGIFactory* pFactory, HWND hWnd)
{
	RECT rect = {};
	GetClientRect(hWnd, &rect);
	m_windowWidth = rect.right - rect.left;
	m_windowHeight = rect.bottom - rect.top;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = s_swapChainBuffersNum;
	swapChainDesc.BufferDesc.Width = m_windowWidth;
	swapChainDesc.BufferDesc.Height = m_windowHeight;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Flags = 0;

	return pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
}

HRESULT Renderer::CreateBackBuffer()
{
	ID3D11Texture2D* pBackBufferTexture = nullptr;

	HRESULT hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBufferTexture));

	if (SUCCEEDED(hr))
	{
		hr = m_pDevice->CreateRenderTargetView(pBackBufferTexture, nullptr, &m_pBackBufferRTV);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC depthTextureDesc = {};
		depthTextureDesc.Usage = D3D11_USAGE_DEFAULT;
		depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthTextureDesc.Width = m_windowWidth;
		depthTextureDesc.Height = m_windowHeight;
		depthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthTextureDesc.ArraySize = 1;
		depthTextureDesc.MipLevels = 1;
		depthTextureDesc.MiscFlags = 0;
		depthTextureDesc.CPUAccessFlags = 0;
		depthTextureDesc.SampleDesc.Count = 1;
		depthTextureDesc.SampleDesc.Quality = 0;

		hr = m_pDevice->CreateTexture2D(&depthTextureDesc, nullptr, &m_pDepthTexture);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Flags = 0;

		m_pDevice->CreateDepthStencilView(m_pDepthTexture, &dsvDesc, &m_pDepthTextureDSV);
	}

	SafeRelease(pBackBufferTexture);

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC hdrTextureDesc = {};
		hdrTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		hdrTextureDesc.Width = m_windowWidth;
		hdrTextureDesc.Height = m_windowHeight;
		hdrTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		hdrTextureDesc.Usage = D3D11_USAGE_DEFAULT;
		hdrTextureDesc.CPUAccessFlags = 0;
		hdrTextureDesc.ArraySize = 1;
		hdrTextureDesc.MipLevels = 1;
		hdrTextureDesc.SampleDesc.Count = 1;
		hdrTextureDesc.SampleDesc.Quality = 0;

		hr = m_pDevice->CreateTexture2D(&hdrTextureDesc, nullptr, &m_pHDRRenderTarget);
	}

	if (SUCCEEDED(hr))
	{
		hr = m_pDevice->CreateRenderTargetView(m_pHDRRenderTarget, nullptr, &m_pHDRTextureRTV);
	}

	if (SUCCEEDED(hr))
	{
		hr = m_pDevice->CreateShaderResourceView(m_pHDRRenderTarget, nullptr, &m_pHDRTextureSRV);
	}

	return hr;
}

HRESULT Renderer::CreatePipelineStateObjects()
{
	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.SlopeScaledDepthBias = 0.0f;
	rasterizerDesc.DepthBiasClamp = 0.0f;
	rasterizerDesc.DepthClipEnable = false;
	rasterizerDesc.ScissorEnable = false;
	rasterizerDesc.MultisampleEnable = false;
	rasterizerDesc.AntialiasedLineEnable = false;

	HRESULT hr = m_pDevice->CreateRasterizerState(&rasterizerDesc, &m_pRasterizerState);

	if (SUCCEEDED(hr))
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc = {};
		depthStencilStateDesc.DepthEnable = true;
		depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthStencilStateDesc.StencilEnable = false;

		hr = m_pDevice->CreateDepthStencilState(&depthStencilStateDesc, &m_pDepthStencilState);
	}

	ID3DBlob* pVSBlob = nullptr;

	if (SUCCEEDED(hr))
	{
		if (m_pShaderCompiler == nullptr)
		{
			m_pShaderCompiler = new ShaderCompiler(m_pDevice, m_isDebug);
		}

		if (!m_pShaderCompiler->CreateVertexAndPixelShaders(
			"shaders/simpleShader.hlsl",
			&m_pVertexShader,
			&pVSBlob,
			&m_pPixelShader
		))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
			CreateInputElementDesc("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0),
			CreateInputElementDesc("COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(DirectX::XMFLOAT3)),
			CreateInputElementDesc("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT4))
		};

		hr = m_pDevice->CreateInputLayout(
			inputLayoutDesc,
			_countof(inputLayoutDesc),
			pVSBlob->GetBufferPointer(),
			pVSBlob->GetBufferSize(),
			&m_pInputLayout
		);
	}

	return hr;
}

HRESULT Renderer::CreateCubeResourses()
{
	static constexpr Vertex vertices[] = {
		{ { -0.5f, -0.5f, 0.5f },	{ 1.0f, 0.0f, 0.0f, 1.0f },	{ 0.0f, -1.0f, 0.0f } },
		{ { 0.5f, -0.5f, 0.5f },	{ 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
		{ { 0.5f, -0.5f, -0.5f },	{ 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
		{ { -0.5f, -0.5f, -0.5f },	{ 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },

		{ { -0.5f, 0.5f, -0.5f },	{ 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 0.5f, 0.5f, -0.5f },	{ 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 0.5f, 0.5f, 0.5f },		{ 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { -0.5f, 0.5f, 0.5f },	{ 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },

		{ { 0.5f, -0.5f, -0.5f },	{ 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.5f, -0.5f, 0.5f },	{ 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.5f, 0.5f, 0.5f },		{ 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.5f, 0.5f, -0.5f },	{ 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },

		{ { -0.5f, -0.5f, 0.5f },	{ 1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
		{ { -0.5f, -0.5f, -0.5f },	{ 1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
		{ { -0.5f, 0.5f, -0.5f },	{ 1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
		{ { -0.5f, 0.5f, 0.5f },	{ 1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },

		{ { 0.5f, -0.5f, 0.5f },	{ 0.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.5f },	{ 0.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, 0.5f },	{ 0.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, 0.5f, 0.5f },		{ 0.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },

		{ { -0.5f, -0.5f, -0.5f },	{ 1.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
		{ { 0.5f, -0.5f, -0.5f },	{ 1.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
		{ { 0.5f, 0.5f, -0.5f },	{ 1.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
		{ { -0.5f, 0.5f, -0.5f },	{ 1.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }
	};

	static constexpr UINT16 indices[] = {
		0, 2, 1, 0, 3, 2,
		4, 6, 5, 4, 7, 6,
		8, 10, 9, 8, 11, 10,
		12, 14, 13, 12, 15, 14,
		16, 18, 17, 16, 19, 18,
		20, 22, 21, 20, 23, 22
	};

	m_cubeIndexCount = _countof(indices);
	D3D11_BUFFER_DESC vertexBufferDesc = CreateDefaultBufferDesc(_countof(vertices) * sizeof(Vertex), D3D11_BIND_VERTEX_BUFFER);
	D3D11_SUBRESOURCE_DATA vertexBufferData = CreateDefaultSubresourceData(&vertices);

	HRESULT hr = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_pCubeVertexBuffer);
	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(_countof(indices) * sizeof(UINT16), D3D11_BIND_INDEX_BUFFER);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(&indices);
		hr = m_pDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_pCubeIndexBuffer);
	}
	return hr;
}

HRESULT Renderer::CreatePlaneResourses()
{
	static constexpr Vertex vertices[] = {
	{ { -0.5f, 0.0f, -0.5f },	{ 0.2f, 0.2f, 0.2f, 1.0f },	{ 0.0f, 1.0f, 0.0f } },
	{ { -0.5f, 0.0f, 0.5f },	{ 0.2f, 0.2f, 0.2f, 1.0f },	{ 0.0f, 1.0f, 0.0f } },
	{ { 0.5f, 0.0f, 0.5f },		{ 0.2f, 0.2f, 0.2f, 1.0f },	{ 0.0f, 1.0f, 0.0f } },
	{ { 0.5f, 0.0f, -0.5f },	{ 0.2f, 0.2f, 0.2f, 1.0f },	{ 0.0f, 1.0f, 0.0f } },
	};

	static constexpr UINT16 indices[] = {
		0, 1, 2, 0, 2, 3
	};

	m_planeIndexCount = _countof(indices);
	D3D11_BUFFER_DESC vertexBufferDesc = CreateDefaultBufferDesc(_countof(vertices) * sizeof(Vertex), D3D11_BIND_VERTEX_BUFFER);
	D3D11_SUBRESOURCE_DATA vertexBufferData = CreateDefaultSubresourceData(&vertices);

	HRESULT hr = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_pPlaneVertexBuffer);
	if (SUCCEEDED(hr)) {
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(m_planeIndexCount * sizeof(UINT16), D3D11_BIND_INDEX_BUFFER);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(&indices);
		hr = m_pDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_pPlaneIndexBuffer);
	}
	return hr;
}

HRESULT Renderer::CreateSceneResources()
{

	HRESULT hr = CreateCubeResourses();
	if (SUCCEEDED(hr))
	{
		hr = CreatePlaneResourses();
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC constantBufferDesc = CreateDefaultBufferDesc(sizeof(ConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

		hr = m_pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pCubeConstantBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC constantBufferDesc = CreateDefaultBufferDesc(sizeof(ConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

		hr = m_pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pPlaneConstantBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC lightBufferDesc = CreateDefaultBufferDesc(sizeof(LightBuffer), D3D11_BIND_CONSTANT_BUFFER);

		m_lights.push_back(PointLight({ -4.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, 1.0f));
		m_lights.push_back(PointLight({ 4.0f, 0.0f, -4.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, 1.0f));
		m_lights.push_back(PointLight({ 0.0f, 0.0f, 4.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, 10.0f));

		LightBuffer lightBuffer = {};
		lightBuffer.lightsCount.x = (UINT)m_lights.size();
		memcpy(lightBuffer.lights, m_lights.data(), sizeof(PointLight) * m_lights.size());

		D3D11_SUBRESOURCE_DATA lightBufferData = {};
		lightBufferData.pSysMem = &lightBuffer;
		lightBufferData.SysMemPitch = 0;
		lightBufferData.SysMemSlicePitch = 0;

		hr = m_pDevice->CreateBuffer(&lightBufferDesc, &lightBufferData, &m_pLightBuffer);
	}

	if (SUCCEEDED(hr))
	{
		m_pToneMapping = ToneMapping::CreateToneMapping(m_pDevice, m_pContext, m_pShaderCompiler, m_windowWidth, m_windowHeight);

		if (m_pToneMapping == nullptr)
		{
			hr = E_FAIL;
		}
	}

	return hr;
}

HRESULT Renderer::SetResourceName(ID3D11Resource* pResource, const std::string& name)
{
	if (!pResource) 
	{
		return E_FAIL;
	}
	return pResource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.size(), name.c_str());
}


bool Renderer::Resize(UINT newWidth, UINT newHeight)
{
	if (m_windowWidth == newWidth && m_windowHeight == newHeight)
	{
		return true;
	}

	SafeRelease(m_pBackBufferRTV);
	SafeRelease(m_pDepthTextureDSV);
	SafeRelease(m_pDepthTexture);
	SafeRelease(m_pHDRRenderTarget);
	SafeRelease(m_pHDRTextureRTV);
	SafeRelease(m_pHDRTextureSRV);

	HRESULT hr = m_pSwapChain->ResizeBuffers(s_swapChainBuffersNum, newWidth, newHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0);

	if (SUCCEEDED(hr))
	{
		m_windowWidth = newWidth;
		m_windowHeight = newHeight;

		hr = CreateBackBuffer();
	}

	return SUCCEEDED(hr);
}

Camera* Renderer::getCamera()
{
	return m_pCamera;
}


void Renderer::FillLightBuffer()
{
	static LightBuffer lightBuffer = {};
	lightBuffer.lightsCount.x = (UINT)m_lights.size();
	memcpy(lightBuffer.lights, m_lights.data(), sizeof(PointLight) * m_lights.size());

	m_pContext->UpdateSubresource(m_pLightBuffer, 0, nullptr, &lightBuffer, 0, 0);
}


void Renderer::Update()
{
	size_t time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	
	if (m_startTime == 0)
	{
		m_startTime = time;
		m_currentTime = time;
	}
	
	m_timeFromLastFrame = time - m_currentTime;
	m_currentTime = time;
	
	FLOAT width = s_near / tanf(s_fov / 2.0f);
	FLOAT height = ((FLOAT)m_windowHeight / m_windowWidth) * width;

	DirectX::XMMATRIX modelCubeMatrix = DirectX::XMMatrixRotationY(s_PI * (m_currentTime - m_startTime) / 10e6f) * DirectX::XMMatrixTranslation(-7.5f, 0.0f, 0.0f);
	DirectX::XMMATRIX modelPlaneMatrix = DirectX::XMMatrixTranslation(0.0f, -2.0f, 0.0f) * DirectX::XMMatrixScaling(10.0f, 1.0f, 10.0f);
	DirectX::XMMATRIX projMatrix = DirectX::XMMatrixPerspectiveLH(width, height, s_near, s_far);
	DirectX::XMMATRIX viewMatrix = m_pCamera->GetViewMatrix();

	ConstantBuffer constantBuffer = {};

	DirectX::XMStoreFloat4x4(&constantBuffer.modelMatrix, DirectX::XMMatrixTranspose(modelCubeMatrix));
	DirectX::XMStoreFloat4x4(&constantBuffer.vpMatrix, DirectX::XMMatrixTranspose(viewMatrix * projMatrix));
	m_pContext->UpdateSubresource(m_pCubeConstantBuffer, 0, nullptr, &constantBuffer, 0, 0);

	DirectX::XMStoreFloat4x4(&constantBuffer.modelMatrix, DirectX::XMMatrixTranspose(modelPlaneMatrix));
	DirectX::XMStoreFloat4x4(&constantBuffer.vpMatrix, DirectX::XMMatrixTranspose(viewMatrix * projMatrix));
	m_pContext->UpdateSubresource(m_pPlaneConstantBuffer, 0, nullptr, &constantBuffer, 0, 0);
}


void Renderer::Render()
{
	Update();

	m_pAnnotation->BeginEvent(L"Draw Cube");

	m_pContext->ClearState();

	m_pContext->OMSetRenderTargets(1, &m_pHDRTextureRTV, m_pDepthTextureDSV);

	static constexpr float fillColor[4] = { 0.3f, 0.4f, 0.3f, 1.0f };
	m_pContext->ClearRenderTargetView(m_pBackBufferRTV, fillColor);
	m_pContext->ClearRenderTargetView(m_pHDRTextureRTV, fillColor);
	m_pContext->ClearDepthStencilView(m_pDepthTextureDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftY = 0;
	viewport.TopLeftX = 0;
	viewport.Width = (FLOAT)m_windowWidth;
	viewport.Height = (FLOAT)m_windowHeight;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;
	rect.right = m_windowWidth;
	rect.bottom = m_windowHeight;

	m_pContext->RSSetViewports(1, &viewport);
	m_pContext->RSSetScissorRects(1, &rect);

	RenderScene();

	m_pAnnotation->EndEvent();

	PostProcessing();

	m_pSwapChain->Present(0, 0);
}

void Renderer::RenderScene()
{
	ID3D11Buffer* vertexBuffers[] = { m_pCubeVertexBuffer };
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pContext->IASetInputLayout(m_pInputLayout);

	m_pContext->RSSetState(m_pRasterizerState);
	m_pContext->OMSetDepthStencilState(m_pDepthStencilState, 0);

	m_pContext->VSSetShader(m_pVertexShader, nullptr, 0);
	m_pContext->PSSetShader(m_pPixelShader, nullptr, 0);


	m_pContext->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	m_pContext->IASetIndexBuffer(m_pCubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	ID3D11Buffer* constantBuffers[] = { m_pCubeConstantBuffer, m_pLightBuffer };

	m_pContext->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
	m_pContext->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);

	m_pContext->DrawIndexed(m_cubeIndexCount, 0, 0);


	ID3D11Buffer* vertexBuffers1[1] = { m_pPlaneVertexBuffer };

	m_pContext->IASetVertexBuffers(0, 1, vertexBuffers1, &stride, &offset);
	m_pContext->IASetIndexBuffer(m_pPlaneIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	ID3D11Buffer* constantBuffers1[] = { m_pPlaneConstantBuffer, m_pLightBuffer };

	m_pContext->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers1);
	m_pContext->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers1);

	m_pContext->DrawIndexed(m_planeIndexCount, 0, 0);
}

void Renderer::PostProcessing()
{
	m_pToneMapping->ToneMap(m_pHDRTextureSRV, m_pBackBufferRTV, m_windowWidth, m_windowHeight, m_timeFromLastFrame / 10e6f);
}
