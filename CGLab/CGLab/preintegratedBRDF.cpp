#include "preintegratedBRDF.h"

PreintegratedBRDFBuilder::~PreintegratedBRDFBuilder()
{
	SafeRelease(m_pTmpTexture);
	SafeRelease(m_pTmpTextureRTV);
	SafeRelease(m_pPreintegratedBRDFVS);
	SafeRelease(m_pPreintegratedBRDFPS);
	SafeRelease(m_pRasterizerState);
}

PreintegratedBRDFBuilder::PreintegratedBRDFBuilder(RendererContext* pContext, UINT maxTextureSize)
	: m_pContext(pContext)
	, m_pTmpTexture(nullptr)
	, m_pTmpTextureRTV(nullptr)
	, m_pPreintegratedBRDFVS(nullptr)
	, m_pPreintegratedBRDFPS(nullptr)
	, m_pRasterizerState(nullptr)
	, m_preintegratedBRDFsize(maxTextureSize)
{}

PreintegratedBRDFBuilder* PreintegratedBRDFBuilder::Create(RendererContext* pContext, UINT maxTextureSize)
{
	PreintegratedBRDFBuilder* pPreintegratedBRDFBuilder = new PreintegratedBRDFBuilder(pContext, maxTextureSize);

	if (pPreintegratedBRDFBuilder != nullptr)
	{
		HRESULT hr = pPreintegratedBRDFBuilder->CreatePipelineStateObjects();

		if (SUCCEEDED(hr))
		{
			hr = pPreintegratedBRDFBuilder->CreateResources();
		}

		if (SUCCEEDED(hr))
		{
			return pPreintegratedBRDFBuilder;
		}
	}

	delete pPreintegratedBRDFBuilder;
	return nullptr;
}

HRESULT PreintegratedBRDFBuilder::CalculatePreintegratedBRDF(
	ID3D11Texture2D** ppPBRDFTexture,
	ID3D11ShaderResourceView** ppPBRDFTextureSRV,
	UINT textureSize
)
{
	if (textureSize > m_preintegratedBRDFsize)
	{
		return E_FAIL;
	}

	ID3D11Device* pDevice = m_pContext->GetDevice();
	
	D3D11_TEXTURE2D_DESC PBRDFTextureDesc = CreateDefaultTexture2DDesc(
		DXGI_FORMAT_R32G32_FLOAT,
		textureSize, textureSize,
		D3D11_BIND_SHADER_RESOURCE
	);

	HRESULT hr = pDevice->CreateTexture2D(&PBRDFTextureDesc, nullptr, ppPBRDFTexture);
	
	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateShaderResourceView(*ppPBRDFTexture, nullptr, ppPBRDFTextureSRV);
	}

	if (SUCCEEDED(hr))
	{
		RenderPreintegratedBRDF(*ppPBRDFTexture, textureSize);
	}

	return hr;
}

void PreintegratedBRDFBuilder::RenderPreintegratedBRDF(ID3D11Texture2D* pTargetTexture, UINT targetSize)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	m_pContext->BeginEvent(L"Preintegrated BRDF");

	pContext->ClearState();
	pContext->OMSetRenderTargets(1, &m_pTmpTextureRTV, nullptr);
	
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftY = 0;
	viewport.TopLeftX = 0;
	viewport.Width = (FLOAT)targetSize;
	viewport.Height = (FLOAT)targetSize;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;
	rect.right = targetSize;
	rect.bottom = targetSize;

	D3D11_BOX srcBox = {};
	srcBox.left = 0;
	srcBox.top = 0;
	srcBox.front = 0;
	srcBox.right = targetSize;
	srcBox.bottom = targetSize;
	srcBox.back = 1;

	pContext->RSSetViewports(1, &viewport);
	pContext->RSSetScissorRects(1, &rect);

	pContext->RSSetState(m_pRasterizerState);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(nullptr);

	pContext->VSSetShader(m_pPreintegratedBRDFVS, nullptr, 0);
	pContext->PSSetShader(m_pPreintegratedBRDFPS, nullptr, 0);

	pContext->Draw(4, 0);

	pContext->CopySubresourceRegion(
		pTargetTexture,
		0,
		0, 0, 0,
		m_pTmpTexture,
		0,
		&srcBox
	);

	m_pContext->EndEvent();
}

HRESULT PreintegratedBRDFBuilder::CreatePipelineStateObjects()
{
	HRESULT hr = S_OK;
	ID3DBlob* pVSBlob = nullptr;

	if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
		"shaders/preintegratedBRDF.hlsl",
		&m_pPreintegratedBRDFVS,
		&pVSBlob,
		&m_pPreintegratedBRDFPS
	))
	{
		hr = E_FAIL;
	}

	SafeRelease(pVSBlob);

	if(SUCCEEDED(hr))
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

		HRESULT hr = m_pContext->GetDevice()->CreateRasterizerState(&rasterizerDesc, &m_pRasterizerState);
	}

	return hr;	
}

HRESULT PreintegratedBRDFBuilder::CreateResources()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	D3D11_TEXTURE2D_DESC PBRDFTextureDesc = CreateDefaultTexture2DDesc(
		DXGI_FORMAT_R32G32_FLOAT,
		m_preintegratedBRDFsize, m_preintegratedBRDFsize,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
	);

	HRESULT hr = pDevice->CreateTexture2D(&PBRDFTextureDesc, nullptr, &m_pTmpTexture);

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRenderTargetView(m_pTmpTexture, nullptr, &m_pTmpTextureRTV);
	}

	return hr;
}
