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

#include "imGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"


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

	DirectX::XMUINT4 pbrMode; // r : 1 - Normal Distribution, 2 - Geometry, 3 - Fresnel, Overwise - All
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
	: m_pContext(nullptr)
	, m_pSwapChain(nullptr)
	, m_pBackBufferRTV(nullptr)
	, m_pDepthTexture(nullptr)
	, m_pDepthTextureDSV(nullptr)
	, m_pHDRRenderTarget(nullptr)
	, m_pHDRTextureRTV(nullptr)
	, m_pHDRTextureSRV(nullptr)
	, m_pRasterizerState(nullptr)
	, m_pRasterizerStateFront(nullptr)
	, m_pDepthStencilState(nullptr)
	, m_pLightBuffer(nullptr)
	, m_pVertexShader(nullptr)
	, m_pPixelShader(nullptr)
	, m_pEnvironmentVShader(nullptr)
	, m_pEnvironmentPShader(nullptr)
	, m_pInputLayout(nullptr)
	, m_pConstantBuffer(nullptr)
	, m_pMinMagLinearSampler(nullptr)
	, m_pEnvironmentCubeMap(nullptr)
	, m_pEnvironmentCubeMapSRV(nullptr)
	, m_pEnvironmentSphere(nullptr)
	, m_pPBRBuffer(nullptr)
	, m_windowWidth(0)
	, m_windowHeight(0)
	, m_projMatrix(DirectX::XMMatrixIdentity())
	, m_startTime(0)
	, m_currentTime(0)
	, m_timeFromLastFrame(0)
	, m_pCamera(nullptr)
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
		m_pContext = RendererContext::CreateContext(pFactory);

		if (m_pContext == nullptr)
		{
			hr = E_FAIL;
		}
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

	if (res)
	{
		res = InitImGui(hWnd);
	}
	
	if (!res)
	{
		Release();
	}

	SafeRelease(pFactory);

	return res;
}

bool Renderer::InitImGui(HWND hWnd)
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	bool res = ImGui_ImplWin32_Init(hWnd);
	if (res)
	{
		res = ImGui_ImplDX11_Init(m_pContext->GetDevice(), m_pContext->GetContext());
	}
	ImGui::StyleColorsDark();
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
	SafeRelease(m_pMinMagLinearSampler);
	SafeRelease(m_pEnvironmentPShader);
	SafeRelease(m_pEnvironmentVShader);
	SafeRelease(m_pLightBuffer);
	SafeRelease(m_pDepthStencilState);
	SafeRelease(m_pRasterizerState);
	SafeRelease(m_pRasterizerStateFront);
	SafeRelease(m_pHDRTextureSRV);
	SafeRelease(m_pHDRTextureRTV);
	SafeRelease(m_pHDRRenderTarget);
	SafeRelease(m_pDepthTextureDSV);
	SafeRelease(m_pDepthTexture);
	SafeRelease(m_pBackBufferRTV);
	SafeRelease(m_pSwapChain);
	SafeRelease(m_pConstantBuffer);

	delete m_pEnvironmentSphere;
	delete m_pToneMapping;
	delete m_pCamera;

	for (auto& mesh : m_meshes)
	{
		delete mesh;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	delete m_pContext;
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

	return pFactory->CreateSwapChain(m_pContext->GetDevice(), &swapChainDesc, &m_pSwapChain);
}

HRESULT Renderer::CreateBackBuffer()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();
	ID3D11Texture2D* pBackBufferTexture = nullptr;

	HRESULT hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBufferTexture));

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRenderTargetView(pBackBufferTexture, nullptr, &m_pBackBufferRTV);
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

		hr = pDevice->CreateTexture2D(&depthTextureDesc, nullptr, &m_pDepthTexture);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Flags = 0;

		pDevice->CreateDepthStencilView(m_pDepthTexture, &dsvDesc, &m_pDepthTextureDSV);
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

		hr = pDevice->CreateTexture2D(&hdrTextureDesc, nullptr, &m_pHDRRenderTarget);
	}

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRenderTargetView(m_pHDRRenderTarget, nullptr, &m_pHDRTextureRTV);
	}

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateShaderResourceView(m_pHDRRenderTarget, nullptr, &m_pHDRTextureSRV);
	}

	return hr;
}

HRESULT Renderer::CreatePipelineStateObjects()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

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

	HRESULT hr = pDevice->CreateRasterizerState(&rasterizerDesc, &m_pRasterizerState);

	if (SUCCEEDED(hr))
	{
		D3D11_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_FRONT;
		rasterizerDesc.FrontCounterClockwise = false;
		rasterizerDesc.DepthBias = 0;
		rasterizerDesc.SlopeScaledDepthBias = 0.0f;
		rasterizerDesc.DepthBiasClamp = 0.0f;
		rasterizerDesc.DepthClipEnable = false;
		rasterizerDesc.ScissorEnable = false;
		rasterizerDesc.MultisampleEnable = false;
		rasterizerDesc.AntialiasedLineEnable = false;

		hr = pDevice->CreateRasterizerState(&rasterizerDesc, &m_pRasterizerStateFront);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = pDevice->CreateSamplerState(&samplerDesc, &m_pMinMagLinearSampler);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc = {};
		depthStencilStateDesc.DepthEnable = true;
		depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthStencilStateDesc.StencilEnable = false;

		hr = pDevice->CreateDepthStencilState(&depthStencilStateDesc, &m_pDepthStencilState);
	}

	ID3DBlob* pVSBlob = nullptr;

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
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
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/environment.hlsl",
			&m_pEnvironmentVShader,
			&pVSBlob,
			&m_pEnvironmentPShader
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

		hr = pDevice->CreateInputLayout(
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

	HRESULT hr = m_pContext->GetDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &mesh->pVertexBuffer);
	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(mesh->indexCount * sizeof(UINT16), D3D11_BIND_INDEX_BUFFER);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(&indices);
		hr = m_pContext->GetDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &mesh->pIndexBuffer);
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

	HRESULT hr = m_pContext->GetDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &mesh->pVertexBuffer);
	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(mesh->indexCount * sizeof(UINT16), D3D11_BIND_INDEX_BUFFER);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(&indices);
		hr = m_pContext->GetDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &mesh->pIndexBuffer);
	}

	if (SUCCEEDED(hr))
	{
		mesh->modelMatrix = DirectX::XMMatrixTranslation(0.0f, -2.0f, 0.0f) * DirectX::XMMatrixScaling(15.0f, 1.0f, 15.0f);
		planeMesh = mesh;
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
		hr = m_pContext->CreateSphereMesh(30, 30, mesh);
		mesh->modelMatrix = DirectX::XMMatrixTranslation(0.0f, 1.0f, 0.0f);
	}

	if (SUCCEEDED(hr))
	{
		m_meshes.push_back(mesh);
		hr = m_pContext->CreateSphereMesh(30, 30, m_pEnvironmentSphere);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC constantBufferDesc = CreateDefaultBufferDesc(sizeof(ConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

		hr = m_pContext->GetDevice()->CreateBuffer(&constantBufferDesc, nullptr, &m_pConstantBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC pbrBufferDesc = CreateDefaultBufferDesc(sizeof(PBRBuffer), D3D11_BIND_CONSTANT_BUFFER);

		PBRBuffer pbrBuffer = {};
		pbrBuffer.albedo = { 1.0f, 0.71f, 0.29f, 1.0f };
		pbrBuffer.roughnessMetalness = { 0.1f, 0.1f, 0.0f, 0.0f };
		pbrBuffer.pbrMode = { 0u, 0u, 0u, 0u };

		D3D11_SUBRESOURCE_DATA pbrBufferData = {};
		pbrBufferData.pSysMem = &pbrBuffer;
		pbrBufferData.SysMemPitch = 0;
		pbrBufferData.SysMemSlicePitch = 0;

		hr = m_pContext->GetDevice()->CreateBuffer(&pbrBufferDesc, &pbrBufferData, &m_pPBRBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC lightBufferDesc = CreateDefaultBufferDesc(sizeof(LightBuffer), D3D11_BIND_CONSTANT_BUFFER);

		m_lights.push_back(PointLight({ 3.0f, 1.0f, -7.5f },	{ 1.0f, 1.0f, 1.0f, 1.0f },	1.0f));

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

		hr = m_pContext->GetDevice()->CreateBuffer(&lightBufferDesc, &lightBufferData, &m_pLightBuffer);
	}

	if (SUCCEEDED(hr))
	{
		m_pToneMapping = ToneMapping::CreateToneMapping(m_pContext, App::MaxWindowSize);

		if (m_pToneMapping == nullptr)
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = m_pContext->LoadTextureCube("data/maskonaive", &m_pEnvironmentCubeMap, &m_pEnvironmentCubeMapSRV);
	}

	if (SUCCEEDED(hr))
	{
		ID3D11Texture2D* pTex = nullptr;
		ID3D11ShaderResourceView* pTexSRV = nullptr;

		hr = m_pContext->LoadTextureCubeFromHDRI("data/hdri/je_gray_02_4k.hdr", &pTex, &pTexSRV);
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

	m_pContext->GetContext()->UpdateSubresource(m_pLightBuffer, 0, nullptr, &lightBuffer, 0, 0);
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
	m_pEnvironmentSphere->modelMatrix = DirectX::XMMatrixTranslation(m_pCamera->GetPosition().x, m_pCamera->GetPosition().y, m_pCamera->GetPosition().z);
}


void Renderer::RenderImGui()
{
	m_pContext->BeginEvent(L"imGui");

	static float bright = 10.0f;
	static bool isNormal = false, isGeometry = false, isFresnel = false, isAll = true;
	static float roughness = 0.1f, metalness = 0.1f, rgb[3] = { 1.0f, 0.71f, 0.29f };
	static UINT pbrMode = 0u;

	static auto updatePBRBuffer = [this]()->void
	{
		PBRBuffer pbrBuffer = {};
		pbrBuffer.roughnessMetalness = { roughness, metalness , 0.0f, 0.0f };
		pbrBuffer.albedo = { rgb[0], rgb[1], rgb[2], 1.0f };
		pbrBuffer.pbrMode = { pbrMode, 0u, 0u, 0u };
		m_pContext->GetContext()->UpdateSubresource(m_pPBRBuffer, 0, nullptr, &pbrBuffer, 0, 0);
	};

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Menu");

	ImGui::SliderFloat("Brightness", &bright, 0.0f, 100.0f);
	ChangeLightBrightness(0, bright);

	ImGui::BeginChild("Display modes", ImVec2(0, 125), true);
	ImGui::Text("Display modes:");

	if (ImGui::Checkbox("Normal distribution function", &isNormal))
	{
		isNormal = true;
		isGeometry = false; 
		isFresnel = false; 
		isAll = false;

		pbrMode = 1;
	}
	if (ImGui::Checkbox("Geometry function", &isGeometry))
	{
		isNormal = false;
		isGeometry = true;
		isFresnel = false;
		isAll = false;

		pbrMode = 2;
	}
	if (ImGui::Checkbox("Fresnel function", &isFresnel))
	{
		isNormal = false;
		isGeometry = false;
		isFresnel = true;
		isAll = false;

		pbrMode = 3;
	}
	if (ImGui::Checkbox("All", &isAll))
	{
		isNormal = false;
		isGeometry = false;
		isFresnel = false;
		isAll = true;

		pbrMode = 0;
	}
	ImGui::EndChild();

	ImGui::BeginChild("PBR setting", ImVec2(0, 100), true);
	ImGui::Text("PBR setting:");

	ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f);
	ImGui::SliderFloat("Metalness", &metalness, 0.0f, 1.0f);
	ImGui::DragFloat3("Albedo", rgb, 0.02f, 0.0f, 1.0f);
	
	ImGui::EndChild();

	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	updatePBRBuffer();

	m_pContext->EndEvent();
}

void Renderer::Render()
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	Update();

	m_pContext->BeginEvent(L"Draw Scene");

	FLOAT width = s_near / tanf(s_fov / 2.0f);
	FLOAT height = ((FLOAT)m_windowHeight / m_windowWidth) * width;
	m_projMatrix = DirectX::XMMatrixPerspectiveLH(width, height, s_near, s_far);

	pContext->ClearState();

	//m_pContext->OMSetRenderTargets(1, &m_pHDRTextureRTV, m_pDepthTextureDSV);
	pContext->OMSetRenderTargets(1, &m_pBackBufferRTV, m_pDepthTextureDSV);

	static constexpr float fillColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	pContext->ClearRenderTargetView(m_pBackBufferRTV, fillColor);
	pContext->ClearRenderTargetView(m_pHDRTextureRTV, fillColor);
	pContext->ClearDepthStencilView(m_pDepthTextureDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

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

	pContext->RSSetViewports(1, &viewport);
	pContext->RSSetScissorRects(1, &rect);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pContext->IASetInputLayout(m_pInputLayout);
	pContext->OMSetDepthStencilState(m_pDepthStencilState, 0);

	RenderEnvironment();
	RenderScene();

	m_pContext->EndEvent();

	//PostProcessing();

	RenderImGui();

	m_pSwapChain->Present(0, 0);
}

void Renderer::RenderScene()
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	pContext->RSSetState(m_pRasterizerState);

	pContext->VSSetShader(m_pVertexShader, nullptr, 0);
	pContext->PSSetShader(m_pPixelShader, nullptr, 0);

	ID3D11Buffer* constantBuffers[] = { m_pConstantBuffer, m_pLightBuffer, m_pPBRBuffer };

	for (auto& mesh : m_meshes)
	{
		ID3D11Buffer* vertexBuffers[] = { mesh->pVertexBuffer };

		pContext->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
		pContext->IASetIndexBuffer(mesh->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

		ConstantBuffer constantBuffer = {};

		DirectX::XMStoreFloat4x4(&constantBuffer.modelMatrix, DirectX::XMMatrixTranspose(mesh->modelMatrix));
		DirectX::XMStoreFloat4x4(&constantBuffer.vpMatrix, DirectX::XMMatrixTranspose(m_pCamera->GetViewMatrix() * m_projMatrix));
		constantBuffer.cameraPosition = m_pCamera->GetPosition();
		pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constantBuffer, 0, 0);

		pContext->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
		pContext->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
		pContext->DrawIndexed(mesh->indexCount, 0, 0);
	}
}

void Renderer::PostProcessing()
{
	m_pContext->BeginEvent(L"Post Processing");

	m_pToneMapping->ToneMap(m_pHDRTextureSRV, m_pBackBufferRTV, m_windowWidth, m_windowHeight, m_timeFromLastFrame / 10e6f);

	m_pContext->EndEvent();
}

void Renderer::RenderEnvironment()
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	m_pContext->BeginEvent(L"Environment");

	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	ID3D11Buffer* constantBuffers[] = { m_pConstantBuffer };

	pContext->PSSetShaderResources(0, 1, &m_pEnvironmentCubeMapSRV);
	pContext->RSSetState(m_pRasterizerStateFront);
	pContext->PSSetSamplers(0, 1, &m_pMinMagLinearSampler);

	pContext->VSSetShader(m_pEnvironmentVShader, nullptr, 0);
	pContext->PSSetShader(m_pEnvironmentPShader, nullptr, 0);

	ID3D11Buffer* vertexBuffers[] = { m_pEnvironmentSphere->pVertexBuffer };

	pContext->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	pContext->IASetIndexBuffer(m_pEnvironmentSphere->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	ConstantBuffer constantBuffer = {};

	DirectX::XMStoreFloat4x4(&constantBuffer.modelMatrix, DirectX::XMMatrixTranspose(m_pEnvironmentSphere->modelMatrix));
	DirectX::XMStoreFloat4x4(&constantBuffer.vpMatrix, DirectX::XMMatrixTranspose(m_pCamera->GetViewMatrix() * m_projMatrix));
	constantBuffer.cameraPosition = m_pCamera->GetPosition();
	pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constantBuffer, 0, 0);

	pContext->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
	pContext->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
	pContext->DrawIndexed(m_pEnvironmentSphere->indexCount, 0, 0);

	m_pContext->EndEvent();
}

