#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <DirectXMath.h>

#include <cmath>
#include <chrono>

#include "WICTextureLoader.h"

#include "renderer.h"

#include "shaderCompiler.h"
#include "toneMapping.h"
#include "camera.h"
#include "app.h"


struct ConstantBuffer
{
	DirectX::XMFLOAT4X4 modelMatrix;
	DirectX::XMFLOAT4X4 vpMatrix;
	DirectX::XMFLOAT4 cameraPosition;
};

struct LightBuffer
{
	DirectX::XMUINT4 lightsCount; // r
	PointLight lights[MaxLightNum];
};

struct PBRBuffer
{
	DirectX::XMFLOAT4 albedo;
	DirectX::XMFLOAT4 roughnessMetalness; // r - roughness, g - metalness
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

void Renderer::DeleteRenderer(Renderer*& pRenderer)
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
	, m_pVertexShader(nullptr)
	, m_pPixelShader(nullptr)
	, m_pInputLayout(nullptr)
	, m_pConstantBuffer(nullptr)
	, m_pEnvironmentCubeMap(nullptr)
	, m_pEnvironmentCubeMapSRV(nullptr)
	, m_pPBRBuffer(nullptr)
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

	if (res)
	{
		m_pCamera = new Camera();
		if (!m_pCamera)
		{
			res = false;
		}
	}

	if (!res)
	{
		Release();
	}

	SafeRelease(pFactory);

	return res;
}


void Renderer::Release()
{
	SafeRelease(m_pPBRBuffer);
	SafeRelease(m_pEnvironmentCubeMapSRV);
	SafeRelease(m_pEnvironmentCubeMap);
	SafeRelease(m_pInputLayout);
	SafeRelease(m_pPixelShader);
	SafeRelease(m_pVertexShader);
	SafeRelease(m_pLightBuffer);
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
	SafeRelease(m_pConstantBuffer);

	delete m_pShaderCompiler;
	delete m_pToneMapping;
	delete m_pCamera;

	for (auto& mesh : m_meshes)
	{
		delete mesh;
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

HRESULT Renderer::CreateCubeResourses(Mesh*& cubeMesh)
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

	Mesh* mesh = new Mesh();
	mesh->indexCount = _countof(indices);

	D3D11_BUFFER_DESC vertexBufferDesc = CreateDefaultBufferDesc(_countof(vertices) * sizeof(Vertex), D3D11_BIND_VERTEX_BUFFER);
	D3D11_SUBRESOURCE_DATA vertexBufferData = CreateDefaultSubresourceData(&vertices);

	HRESULT hr = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &mesh->pVertexBuffer);
	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(mesh->indexCount * sizeof(UINT16), D3D11_BIND_INDEX_BUFFER);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(&indices);
		hr = m_pDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &mesh->pIndexBuffer);
	}

	if (SUCCEEDED(hr))
	{
		mesh->modelMatrix = DirectX::XMMatrixTranslation(-7.5f, 0.0f, 0.0f);
		cubeMesh = mesh;
	}
	else
	{
		delete mesh;
	}

	return hr;
}

HRESULT Renderer::CreatePlaneResourses(Mesh*& planeMesh)
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

	Mesh* mesh = new Mesh();
	mesh->indexCount = _countof(indices);

	D3D11_BUFFER_DESC vertexBufferDesc = CreateDefaultBufferDesc(_countof(vertices) * sizeof(Vertex), D3D11_BIND_VERTEX_BUFFER);
	D3D11_SUBRESOURCE_DATA vertexBufferData = CreateDefaultSubresourceData(&vertices);

	HRESULT hr = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &mesh->pVertexBuffer);
	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(mesh->indexCount * sizeof(UINT16), D3D11_BIND_INDEX_BUFFER);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(&indices);
		hr = m_pDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &mesh->pIndexBuffer);
	}

	if (SUCCEEDED(hr))
	{
		mesh->modelMatrix = DirectX::XMMatrixTranslation(0.0f, -2.0f, 0.0f) * DirectX::XMMatrixScaling(10.0f, 1.0f, 10.0f);
		planeMesh = mesh;
	}
	else
	{
		delete mesh;
	}
	return hr;
}


HRESULT Renderer::CreateSphereResourses(UINT16 latitudeBands, UINT16 longitudeBands, Mesh*& sphereMesh)
{
	std::vector<Vertex> vertices;
	std::vector<UINT16> indices;

	for (UINT16 latNumber = 0; latNumber <= latitudeBands; ++latNumber)
	{
		float theta = latNumber * PI / latitudeBands;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);

		for (UINT16 longNumber = 0; longNumber <= longitudeBands; ++longNumber)
		{
			float phi = longNumber * 2 * PI / longitudeBands;
			float sinPhi = sin(phi);
			float cosPhi = cos(phi);

			float normalX = cosPhi * sinTheta;
			float normalY = cosTheta;
			float normalZ = sinPhi * sinTheta;

			Vertex vs = { { normalX, normalY, normalZ },	
						  { 0.5f, 0.1f, 0.1f, 1.0f },	
				          { normalX, normalY, normalZ } };

			vertices.push_back(vs);
		}
	}

	for (UINT16 latNumber = 0; latNumber < latitudeBands; ++latNumber)
	{
		for (UINT16 longNumber = 0; longNumber < longitudeBands; ++longNumber)
		{
			UINT16 first = (latNumber * (longitudeBands + 1)) + longNumber;
			UINT16 second = first + longitudeBands + 1;

			indices.push_back(first);
			indices.push_back(first + 1);
			indices.push_back(second);

			indices.push_back(second + 1);
			indices.push_back(second);
			indices.push_back(first + 1);
		}
	}

	Mesh* mesh = new Mesh();
	mesh->indexCount = (UINT)indices.size();

	D3D11_BUFFER_DESC vertexBufferDesc = CreateDefaultBufferDesc((UINT)vertices.size() * sizeof(Vertex), D3D11_BIND_VERTEX_BUFFER);
	D3D11_SUBRESOURCE_DATA vertexBufferData = CreateDefaultSubresourceData(vertices.data());

	HRESULT hr = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &mesh->pVertexBuffer);
	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(mesh->indexCount * sizeof(UINT16), D3D11_BIND_INDEX_BUFFER);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(indices.data());

		hr = m_pDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &mesh->pIndexBuffer);
	}

	if (SUCCEEDED(hr))
	{
		mesh->modelMatrix = DirectX::XMMatrixTranslation(0.0f, 1.0f, 0.0f);
		sphereMesh = mesh;
	}
	else
	{
		delete mesh;
	}

	return hr;
}

HRESULT Renderer::CreateSceneResources()
{
	Mesh* mesh = nullptr;

	HRESULT hr = CreateCubeResourses(mesh);

	if (SUCCEEDED(hr))
	{
		m_meshes.push_back(mesh);
		hr = CreatePlaneResourses(mesh);
	}

	if (SUCCEEDED(hr))
	{
		m_meshes.push_back(mesh);
		hr = CreateSphereResourses(30, 30, mesh);
	}

	if (SUCCEEDED(hr))
	{
		m_meshes.push_back(mesh);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC constantBufferDesc = CreateDefaultBufferDesc(sizeof(ConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

		hr = m_pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pConstantBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC pbrBufferDesc = CreateDefaultBufferDesc(sizeof(PBRBuffer), D3D11_BIND_CONSTANT_BUFFER);

		PBRBuffer pbrBuffer = {};
		pbrBuffer.albedo = { 1.0f, 0.71f, 0.29f, 1.0f };
		pbrBuffer.roughnessMetalness = { 0.1f, 0.1f, 0.0f, 0.0f };

		D3D11_SUBRESOURCE_DATA pbrBufferData = {};
		pbrBufferData.pSysMem = &pbrBuffer;
		pbrBufferData.SysMemPitch = 0;
		pbrBufferData.SysMemSlicePitch = 0;

		hr = m_pDevice->CreateBuffer(&pbrBufferDesc, &pbrBufferData, &m_pPBRBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC lightBufferDesc = CreateDefaultBufferDesc(sizeof(LightBuffer), D3D11_BIND_CONSTANT_BUFFER);

		m_lights.push_back(PointLight({ 3.0f, 1.0f, -7.5f },	{ 1.0f, 1.0f, 0.0f, 1.0f },	1.0f));

		//m_lights.push_back(PointLight({ -4.0f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, 1.0f));
		//m_lights.push_back(PointLight({ 4.0f, -0.25f, -4.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, 1.0f));
		//m_lights.push_back(PointLight({ 0.0f, -0.25f, 4.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, 1.0f));

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
		m_pToneMapping = ToneMapping::CreateToneMapping(m_pDevice, m_pContext, m_pShaderCompiler, App::MaxWindowSize);

		if (m_pToneMapping == nullptr)
		{
			hr = E_FAIL;
		}

		m_pToneMapping->UseAnnotations(m_pAnnotation);
	}

	if (SUCCEEDED(hr))
	{
		hr = LoadTextureCube("data/maskonaive", &m_pEnvironmentCubeMap, &m_pEnvironmentCubeMapSRV);
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

HRESULT Renderer::LoadTextureCube(
	const std::string& pathToCubeSrc,
	ID3D11Texture2D** ppTextureCube,
	ID3D11ShaderResourceView** ppTextureCubeSRV
) const
{
	std::string edges[6] = { "posx", "negx", "posy", "negy", "posz", "negz" };
	HRESULT hr = S_OK;

	ID3D11Texture2D* pSrcTexture = nullptr;

	if (SUCCEEDED(hr))
	{
		std::string edgeName = pathToCubeSrc + "/" + edges[0] + ".jpg";
		std::wstring wEdgeName(edgeName.begin(), edgeName.end());

		hr = CreateWICTextureFromFile(m_pDevice, m_pContext, wEdgeName.c_str(), (ID3D11Resource**)&pSrcTexture, nullptr);
	}

	D3D11_TEXTURE2D_DESC cubeMapDesc = {};
	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC srcTextureDesc = {};
		pSrcTexture->GetDesc(&srcTextureDesc);

		cubeMapDesc.Width = srcTextureDesc.Width;
		cubeMapDesc.Height = srcTextureDesc.Height;
		cubeMapDesc.Format = srcTextureDesc.Format;
		cubeMapDesc.MipLevels = 1;
		cubeMapDesc.ArraySize = 6;
		cubeMapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		cubeMapDesc.Usage = D3D11_USAGE_DEFAULT;
		cubeMapDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		cubeMapDesc.CPUAccessFlags = 0;
		cubeMapDesc.SampleDesc.Count = 1;
		cubeMapDesc.SampleDesc.Quality = 0;

		hr = m_pDevice->CreateTexture2D(&cubeMapDesc, nullptr, ppTextureCube);
	}

	if (SUCCEEDED(hr) && ppTextureCubeSRV != nullptr)
	{
		hr = m_pDevice->CreateShaderResourceView(*ppTextureCube, nullptr, ppTextureCubeSRV);
	}

	if (SUCCEEDED(hr))
	{
		m_pContext->CopySubresourceRegion(*ppTextureCube, 0, 0, 0, 0, pSrcTexture, 0, nullptr);
	}

	SafeRelease(pSrcTexture);

	for (UINT i = 1; i < _countof(edges) && SUCCEEDED(hr); ++i)
	{
		std::string edgeName = pathToCubeSrc + "/" + edges[i] + ".jpg";
		std::wstring wEdgeName(edgeName.begin(), edgeName.end());

		hr = CreateWICTextureFromFile(m_pDevice, m_pContext, wEdgeName.c_str(), (ID3D11Resource**)&pSrcTexture, nullptr);

		if (FAILED(hr))
		{
			break;
		}

		if (m_isDebug)
		{
			D3D11_TEXTURE2D_DESC srcTextureDesc = {};
			pSrcTexture->GetDesc(&srcTextureDesc);

			if (srcTextureDesc.Width != cubeMapDesc.Width
				|| srcTextureDesc.Height != cubeMapDesc.Height
				|| srcTextureDesc.Format != cubeMapDesc.Format)
			{
				hr = E_FAIL;
				break;
			}
		}

		m_pContext->CopySubresourceRegion(*ppTextureCube, i, 0, 0, 0, pSrcTexture, 0, nullptr);

		SafeRelease(pSrcTexture);
	}

	SafeRelease(pSrcTexture);

	return hr;
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

void Renderer::ChangeLightBrightness(UINT lightIdx, FLOAT newBrightness)
{
	assert(lightIdx < m_lights.size());

	m_lights[lightIdx].SetBrightness(newBrightness);
	FillLightBuffer();
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
	
	m_meshes[0]->modelMatrix = DirectX::XMMatrixRotationY(PI * (m_currentTime - m_startTime) / 10e6f) * DirectX::XMMatrixTranslation(-7.5f, 0.0f, 0.0f);
}


void Renderer::Render()
{
	Update();

	m_pAnnotation->BeginEvent(L"Draw Scene");

	m_pContext->ClearState();

	//m_pContext->OMSetRenderTargets(1, &m_pHDRTextureRTV, m_pDepthTextureDSV);
	m_pContext->OMSetRenderTargets(1, &m_pBackBufferRTV, m_pDepthTextureDSV);

	static constexpr float fillColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
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

	//PostProcessing();

	m_pSwapChain->Present(0, 0);
}

void Renderer::RenderScene()
{
	FLOAT width = s_near / tanf(s_fov / 2.0f);
	FLOAT height = ((FLOAT)m_windowHeight / m_windowWidth) * width;

	DirectX::XMMATRIX projMatrix = DirectX::XMMatrixPerspectiveLH(width, height, s_near, s_far);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pContext->IASetInputLayout(m_pInputLayout);

	m_pContext->RSSetState(m_pRasterizerState);
	m_pContext->OMSetDepthStencilState(m_pDepthStencilState, 0);

	m_pContext->VSSetShader(m_pVertexShader, nullptr, 0);
	m_pContext->PSSetShader(m_pPixelShader, nullptr, 0);

	ID3D11Buffer* constantBuffers[] = { m_pConstantBuffer, m_pLightBuffer, m_pPBRBuffer };

	for (auto mesh : m_meshes)
	{
		ID3D11Buffer* vertexBuffers[] = { mesh->pVertexBuffer };

		m_pContext->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
		m_pContext->IASetIndexBuffer(mesh->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

		ConstantBuffer constantBuffer = {};

		DirectX::XMStoreFloat4x4(&constantBuffer.modelMatrix, DirectX::XMMatrixTranspose(mesh->modelMatrix));
		DirectX::XMStoreFloat4x4(&constantBuffer.vpMatrix, DirectX::XMMatrixTranspose(m_pCamera->GetViewMatrix() * projMatrix));
		constantBuffer.cameraPosition = m_pCamera->GetPosition();
		m_pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constantBuffer, 0, 0);

		m_pContext->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
		m_pContext->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
		m_pContext->DrawIndexed(mesh->indexCount, 0, 0);
	}
}

void Renderer::PostProcessing()
{
	m_pAnnotation->BeginEvent(L"Post Processing");

	m_pToneMapping->ToneMap(m_pHDRTextureSRV, m_pBackBufferRTV, m_windowWidth, m_windowHeight, m_timeFromLastFrame / 10e6f);

	m_pAnnotation->EndEvent();
}
