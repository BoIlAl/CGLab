#include "renderer.h"

#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <cmath>

#include "shaderCompiler.h"
#include "common.h"


struct Vertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;
};

struct ConstantBuffer
{
	DirectX::XMFLOAT4X4 vpMatrix;
};


Renderer* Renderer::CreateRenderer(HWND hWnd)
{
	Renderer* pRenderer = new Renderer();

	if (pRenderer->Init(hWnd))
	{
		return pRenderer;
	}

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
	, m_pRasterizerState(nullptr)
	, m_pVertexBuffer(nullptr)
	, m_pIndexBuffer(nullptr)
	, m_indexCount(0)
	, m_pConstantBuffer(nullptr)
	, m_pVertexShader(nullptr)
	, m_pPixelShader(nullptr)
	, m_pInputLayout(nullptr)
	, m_windowWidth(0)
	, m_windowHeight(0)
	, m_pShaderCompiler(nullptr)
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
		hr = CreateRasterizerState();
	}

	if (SUCCEEDED(hr))
	{
		hr = CreateSceneResources();
	}

	bool res = SUCCEEDED(hr);
	
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
	SafeRelease(m_pIndexBuffer);
	SafeRelease(m_pVertexBuffer);
	SafeRelease(m_pConstantBuffer);
	SafeRelease(m_pRasterizerState);
	SafeRelease(m_pBackBufferRTV);
	SafeRelease(m_pSwapChain);
	SafeRelease(m_pContext);
	SafeRelease(m_pDevice);

	if (m_pShaderCompiler != nullptr)
	{
		delete m_pShaderCompiler;
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
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc.Width = m_windowWidth;
	swapChainDesc.BufferDesc.Height = m_windowHeight;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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

	SafeRelease(pBackBufferTexture);

	return hr;
}

HRESULT Renderer::CreateRasterizerState()
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

	return m_pDevice->CreateRasterizerState(&rasterizerDesc, &m_pRasterizerState);
}

HRESULT Renderer::CreateSceneResources()
{
	static constexpr Vertex vertices[] = {
		{ { 0.0f, 0.5f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
	};

	static constexpr UINT16 indices[] = {
		0, 1, 2
	};

	D3D11_BUFFER_DESC vertexBufferDesc = CreateDefaultBufferDesc(_countof(vertices) * sizeof(Vertex), D3D11_BIND_VERTEX_BUFFER);
	D3D11_SUBRESOURCE_DATA vertexBufferData = CreateDefaultSubresourceData(&vertices);

	HRESULT hr = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_pVertexBuffer);

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(_countof(indices) * sizeof(UINT16), D3D11_BIND_INDEX_BUFFER);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(&indices);
		
		m_indexCount = _countof(indices);

		hr = m_pDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_pIndexBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC constantBufferDesc = CreateDefaultBufferDesc(sizeof(ConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

		hr = m_pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pConstantBuffer);
	}

	ID3DBlob* pVSBlob = nullptr;

	if (SUCCEEDED(hr))
	{
		m_pShaderCompiler = new ShaderCompiler(m_pDevice);

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
			CreateInputElementDesc("COLOR", DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(DirectX::XMFLOAT3))
		};

		hr = m_pDevice->CreateInputLayout(
			inputLayoutDesc,
			_countof(inputLayoutDesc),
			pVSBlob->GetBufferPointer(),
			pVSBlob->GetBufferSize(),
			&m_pInputLayout
		);
	}


	return S_OK;
}


void Renderer::Update()
{
	FLOAT width = s_near / tanf(s_fov / 2.0f);
	FLOAT height = ((float)m_windowHeight / m_windowWidth) * width;

	DirectX::XMMATRIX projMatrix = DirectX::XMMatrixPerspectiveLH(width, height, s_near, s_far);
	
	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookToLH(
		DirectX::XMVectorSet(s_cameraPosition[0], s_cameraPosition[1], s_cameraPosition[2], 1.0f),
		DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	);

	ConstantBuffer constantBuffer = {};
	DirectX::XMStoreFloat4x4(&constantBuffer.vpMatrix, DirectX::XMMatrixTranspose(viewMatrix * projMatrix));

	m_pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constantBuffer, 0, 0);
}


void Renderer::Render()
{
	Update();

	m_pContext->ClearState();

	m_pContext->OMSetRenderTargets(1, &m_pBackBufferRTV, nullptr);

	static constexpr float fillColor[4] = { 0.3f, 0.4f, 0.3f, 1.0f };
	m_pContext->ClearRenderTargetView(m_pBackBufferRTV, fillColor);

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

	m_pSwapChain->Present(0, 0);
}

void Renderer::RenderScene()
{
	ID3D11Buffer* vertexBuffers[] = { m_pVertexBuffer };
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	m_pContext->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	m_pContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pContext->IASetInputLayout(m_pInputLayout);

	m_pContext->RSSetState(m_pRasterizerState);

	m_pContext->VSSetShader(m_pVertexShader, nullptr, 0);
	m_pContext->PSSetShader(m_pPixelShader, nullptr, 0);

	ID3D11Buffer* constantBuffers[] = { m_pConstantBuffer };

	m_pContext->VSSetConstantBuffers(0, 1, constantBuffers);
	m_pContext->PSSetConstantBuffers(0, 1, constantBuffers);

	m_pContext->DrawIndexed(m_indexCount, 0, 0);
}
