#include "preintegratedBRDF.h"

PreintegratedBRDF::~PreintegratedBRDF()
{
	SafeRelease(m_pPBRDFTextureRTV);
	SafeRelease(m_pPreintegratedBRDFVS);
	SafeRelease(m_pPreintegratedBRDFPS);
	SafeRelease(m_pRasterizerState);
}

PreintegratedBRDF::PreintegratedBRDF(RendererContext* pContext)
	: m_pContext(pContext)
	, m_pPBRDFTextureRTV(nullptr)
	, m_pPreintegratedBRDFVS(nullptr)
	, m_pPreintegratedBRDFPS(nullptr)
	, m_pRasterizerState(nullptr)
{}

PreintegratedBRDF* PreintegratedBRDF::Create(RendererContext* pContext)
{
	PreintegratedBRDF* pPreintegratedBRDF = new PreintegratedBRDF(pContext);
	if (pPreintegratedBRDF != nullptr)
	{
		HRESULT hr = pPreintegratedBRDF->CreatePipelineStateObjects();
		if (SUCCEEDED(hr))
		{
			return pPreintegratedBRDF;
		}
		else
		{
			delete pPreintegratedBRDF;
			return nullptr;
		}
	}
	return nullptr;
}

HRESULT PreintegratedBRDF::CalculatePreintegratedBRDF(
	ID3D11Texture2D** ppPBRDFTexture,
	ID3D11ShaderResourceView** ppPBRDFTextureSRV
)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();
	ID3D11Device* pDevice = m_pContext->GetDevice();

	m_pContext->BeginEvent(L"Preintegrated BRDF");
	
	D3D11_TEXTURE2D_DESC PBRDFTextureDesc = CreateDefaultTexture2DDesc(
		DXGI_FORMAT_R32G32_FLOAT,
		m_preintegratedBRDFsize, m_preintegratedBRDFsize,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
	);

	HRESULT hr = pDevice->CreateTexture2D(&PBRDFTextureDesc, nullptr, ppPBRDFTexture);
	
	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRenderTargetView(*ppPBRDFTexture, nullptr, &m_pPBRDFTextureRTV);
	}

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateShaderResourceView(*ppPBRDFTexture, nullptr, ppPBRDFTextureSRV);
	}


	if (SUCCEEDED(hr))
	{
		pContext->OMSetRenderTargets(1, &m_pPBRDFTextureRTV, nullptr);

		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftY = 0;
		viewport.TopLeftX = 0;
		viewport.Width = (FLOAT)m_preintegratedBRDFsize;
		viewport.Height = (FLOAT)m_preintegratedBRDFsize;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;
		

		D3D11_RECT rect = {};
		rect.left = 0;
		rect.top = 0;
		rect.right = m_preintegratedBRDFsize;
		rect.bottom = m_preintegratedBRDFsize;

		pContext->RSSetViewports(1, &viewport);
		pContext->RSSetScissorRects(1, &rect);

		pContext->RSSetState(m_pRasterizerState);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);

		pContext->VSSetShader(m_pPreintegratedBRDFVS, nullptr, 0);
		pContext->PSSetShader(m_pPreintegratedBRDFPS, nullptr, 0);

		pContext->Draw(4, 0);
	}

	m_pContext->EndEvent();

	return hr;
}

HRESULT PreintegratedBRDF::CreatePipelineStateObjects()
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
