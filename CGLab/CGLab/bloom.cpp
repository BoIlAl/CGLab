#include "bloom.h"

struct BrightnessThresholdBuffer
{
	DirectX::XMFLOAT4 brightnessThreshold;
};

Bloom* Bloom::Create(RendererContext* pContext)
{
	Bloom* pBloom = new Bloom(pContext);

	HRESULT hr = pBloom->CreatePipelineStateObjects();

	if (SUCCEEDED(hr))
	{
		hr = pBloom->CreateResources();
	}

	if (SUCCEEDED(hr))
	{
		return pBloom;
	}

	delete pBloom;
	return nullptr;
}

Bloom::~Bloom()
{
	SafeRelease(m_pBrightnessThresholdBuffer);
	SafeRelease(m_pRasterizerState);
	SafeRelease(m_pBloomMaskPS);
	SafeRelease(m_pBloomMaskVS);
}

Bloom::Bloom(RendererContext* pContext)
	: m_pContext(pContext)
	, m_pBrightnessThresholdBuffer(nullptr)
	, m_pRasterizerState(nullptr)
	, m_pBloomMaskPS(nullptr)
	, m_pBloomMaskVS(nullptr)
{
}

HRESULT Bloom::CreatePipelineStateObjects()
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

	ID3DBlob* pBlob = nullptr;

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/bloomMask.hlsl",
			&m_pBloomMaskVS,
			&pBlob,
			&m_pBloomMaskPS
		))
		{
			hr = E_FAIL;
		}
	}

	return hr;
}

HRESULT Bloom::CreateResources()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	D3D11_BUFFER_DESC constBufferDesc = CreateDefaultBufferDesc(sizeof(BrightnessThresholdBuffer), D3D11_BIND_CONSTANT_BUFFER);

	auto brightnessThreshold = DirectX::FXMVECTOR({ m_pBrightnessThreshold, 0, 0, 0 });

	D3D11_SUBRESOURCE_DATA constBufferData = CreateDefaultSubresourceData(&brightnessThreshold);

	HRESULT hr = pDevice->CreateBuffer(&constBufferDesc, &constBufferData, &m_pBrightnessThresholdBuffer);

	return hr;
}

HRESULT Bloom::CalculateBloomMask(
	ID3D11ShaderResourceView* pHDRTextureSRV,
	ID3D11ShaderResourceView* pEmissiveSRV,
	UINT renderTargetWidth,
	UINT renderTargetHeight,
	ID3D11Texture2D** ppBloomMask,
	ID3D11ShaderResourceView** ppBloomMaskSRV
)
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	ID3D11RenderTargetView* pBloomMaskRTV = nullptr;

	D3D11_TEXTURE2D_DESC bloomMaskDesc = {};
	bloomMaskDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	bloomMaskDesc.Width = renderTargetWidth;
	bloomMaskDesc.Height = renderTargetHeight;
	bloomMaskDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	bloomMaskDesc.Usage = D3D11_USAGE_DEFAULT;
	bloomMaskDesc.CPUAccessFlags = 0;
	bloomMaskDesc.ArraySize = 1;
	bloomMaskDesc.MipLevels = 0;
	bloomMaskDesc.SampleDesc.Count = 1;
	bloomMaskDesc.SampleDesc.Quality = 0;


	HRESULT hr = pDevice->CreateTexture2D(&bloomMaskDesc, nullptr, ppBloomMask); //

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRenderTargetView(*ppBloomMask, nullptr, &pBloomMaskRTV);
	}

	if (SUCCEEDED(hr))
	{
		RenderBloomMask(
			pHDRTextureSRV, 
			pEmissiveSRV, 
			pBloomMaskRTV, 
			renderTargetWidth, 
			renderTargetHeight
		);

		if (ppBloomMaskSRV != nullptr)
		{
			hr = pDevice->CreateShaderResourceView(*ppBloomMask, nullptr, ppBloomMaskSRV);
		}
	}

	SafeRelease(pBloomMaskRTV);
	return hr;
}

void Bloom::RenderBloomMask(
	ID3D11ShaderResourceView* pHDRTextureSRV,
	ID3D11ShaderResourceView* pEmissiveSRV,
	ID3D11RenderTargetView* pBloomMaskRTV,
	UINT renderTargetWidth,
	UINT renderTargetHeight
)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();
	m_pContext->BeginEvent(L"Bloom Mask");

	pContext->OMSetRenderTargets(1, &pBloomMaskRTV, nullptr);

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftY = 0;
	viewport.TopLeftX = 0;
	viewport.Width = (FLOAT)renderTargetWidth;
	viewport.Height = (FLOAT)renderTargetHeight;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;
	rect.right = renderTargetWidth;
	rect.bottom = renderTargetHeight;

	pContext->RSSetViewports(1, &viewport);
	pContext->RSSetScissorRects(1, &rect);

	pContext->RSSetState(m_pRasterizerState);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(nullptr);

	pContext->VSSetShader(m_pBloomMaskVS, nullptr, 0);
	pContext->PSSetShader(m_pBloomMaskPS, nullptr, 0);

	ID3D11ShaderResourceView* SRVs[] = { pHDRTextureSRV, pEmissiveSRV };

	pContext->PSSetShaderResources(0, _countof(SRVs), SRVs);

	pContext->PSSetConstantBuffers(0, 1, &m_pBrightnessThresholdBuffer);
	
	pContext->Draw(4, 0);

	m_pContext->EndEvent();
}