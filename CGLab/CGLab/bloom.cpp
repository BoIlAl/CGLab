#include "bloom.h"


struct BloomConstantBuffer
{
	DirectX::XMFLOAT4 pixelSizeThreshold;
};


Bloom* Bloom::Create(RendererContext* pContext, UINT targetWidth, UINT targetHeight)
{
	Bloom* pBloom = new Bloom(pContext, targetWidth, targetHeight);

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
	ReleasePingPongResources();

	SafeRelease(m_pMinMagLinearSampler);
	SafeRelease(m_pMinMagMipPointSampler);
	SafeRelease(m_pBloomConstantBuffer);
	SafeRelease(m_pBlendState);
	SafeRelease(m_pRasterizerState);
	SafeRelease(m_pBloomMaskPS);
	SafeRelease(m_pBloomMaskVS);
	SafeRelease(m_pGaussBlurVerticalPS);
	SafeRelease(m_pGaussBlurVerticalVS);
	SafeRelease(m_pGaussBlurHorizontalPS);
	SafeRelease(m_pGaussBlurHorizontalVS);
	SafeRelease(m_pBloomPS);
	SafeRelease(m_pBloomVS);
}

Bloom::Bloom(RendererContext* pContext, UINT targetWidth, UINT targetHeight)
	: m_pContext(pContext)
	, m_width(targetWidth)
	, m_height(targetHeight)
	, m_blurTextureWidth(targetWidth / 2)
	, m_blurTextureHeight(targetHeight / 2)
	, m_pRasterizerState(nullptr)
	, m_pBlendState(nullptr)
	, m_pBloomMaskPS(nullptr)
	, m_pBloomMaskVS(nullptr)
	, m_pGaussBlurVerticalPS(nullptr)
	, m_pGaussBlurVerticalVS(nullptr)
	, m_pGaussBlurHorizontalPS(nullptr)
	, m_pGaussBlurHorizontalVS(nullptr)
	, m_pBloomPS(nullptr)
	, m_pBloomVS(nullptr)
	, m_pBloomConstantBuffer(nullptr)
	, m_pMinMagMipPointSampler(nullptr)
	, m_pMinMagLinearSampler(nullptr)
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

	if (SUCCEEDED(hr))
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;
		blendDesc.RenderTarget[0].BlendEnable = true;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;

		hr = m_pContext->GetDevice()->CreateBlendState(&blendDesc, &m_pBlendState);
	}

	ID3DBlob* pBlob = nullptr;

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/bloom.hlsl",
			&m_pBloomMaskVS,
			&pBlob,
			&m_pBloomMaskPS,
			"BLOOM_MASK=1"
		))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/bloom.hlsl",
			&m_pGaussBlurVerticalVS,
			&pBlob,
			&m_pGaussBlurVerticalPS,
			"GAUSS_BLUR_SEPARATED_VERTICAL=1"
		))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/bloom.hlsl",
			&m_pGaussBlurHorizontalVS,
			&pBlob,
			&m_pGaussBlurHorizontalPS,
			"GAUSS_BLUR_SEPARATED_HORIZONTAL=1"
		))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/bloom.hlsl",
			&m_pBloomVS,
			&pBlob,
			&m_pBloomPS
		))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = m_pContext->GetDevice()->CreateSamplerState(&samplerDesc, &m_pMinMagMipPointSampler);

		if (SUCCEEDED(hr))
		{
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;

			hr = m_pContext->GetDevice()->CreateSamplerState(&samplerDesc, &m_pMinMagLinearSampler);
		}
	}

	return hr;
}

HRESULT Bloom::CreateResources()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	D3D11_BUFFER_DESC brighnessThresholdBufferDesc = CreateDefaultBufferDesc(sizeof(BloomConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

	BloomConstantBuffer bloomConstBuffer = {};
	bloomConstBuffer.pixelSizeThreshold = { 1.0f / m_blurTextureWidth, 1.0f / m_blurTextureHeight, m_brightnessThreshold, 0.0f };

	D3D11_SUBRESOURCE_DATA constBufferData = CreateDefaultSubresourceData(&bloomConstBuffer);

	HRESULT hr = pDevice->CreateBuffer(&brighnessThresholdBufferDesc, &constBufferData, &m_pBloomConstantBuffer);

	if (SUCCEEDED(hr))
	{
		hr = UpdatePingPongTextures();
	}

	return hr;
}

HRESULT Bloom::UpdatePingPongTextures()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	HRESULT hr = S_OK;

	D3D11_TEXTURE2D_DESC textureDesc = CreateDefaultTexture2DDesc(
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		m_blurTextureWidth, m_blurTextureHeight,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET
	);

	for (UINT i = 0; i < _countof(m_pingPongBlurTextures) && SUCCEEDED(hr); ++i)
	{
		hr = pDevice->CreateTexture2D(&textureDesc, nullptr, &m_pingPongBlurTextures[i]);

		if (SUCCEEDED(hr))
		{
			hr = pDevice->CreateShaderResourceView(m_pingPongBlurTextures[i], nullptr, &m_pingPongBlurTexturesSRV[i]);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDevice->CreateRenderTargetView(m_pingPongBlurTextures[i], nullptr, &m_pingPongBlurTexturesRTV[i]);
		}
	}

	return hr;
}


HRESULT Bloom::Resize(UINT newTargetWidth, UINT newTargetHeight)
{
	ReleasePingPongResources();

	m_width = newTargetWidth;
	m_height = newTargetHeight;
	m_blurTextureWidth = m_width / 2;
	m_blurTextureHeight = m_height / 2;

	HRESULT hr = UpdatePingPongTextures();

	if (SUCCEEDED(hr))
	{
		BloomConstantBuffer bloomConstBuffer = {};
		bloomConstBuffer.pixelSizeThreshold = { 1.0f / m_blurTextureWidth, 1.0f / m_blurTextureHeight, m_brightnessThreshold, 0.0f };

		m_pContext->GetContext()->UpdateSubresource(m_pBloomConstantBuffer, 0, nullptr, &bloomConstBuffer, 0, 0);
	}

	return hr;
}


void Bloom::ReleasePingPongResources()
{
	for (UINT i = 0; i < 2; ++i)
	{
		SafeRelease(m_pingPongBlurTextures[i]);
		SafeRelease(m_pingPongBlurTexturesSRV[i]);
		SafeRelease(m_pingPongBlurTexturesRTV[i]);
	}
}


void Bloom::CalculateBloom(
	ID3D11ShaderResourceView* pHDRTextureSRV,
	ID3D11ShaderResourceView* pEmissiveSRV,
	ID3D11RenderTargetView* pTargetTextureRTV
)
{
	m_pContext->BeginEvent(L"Bloom");

	ID3D11DeviceContext* pContext = m_pContext->GetContext();
	pContext->ClearState();

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftY = 0;
	viewport.TopLeftX = 0;
	viewport.Width = (FLOAT)m_blurTextureWidth;
	viewport.Height = (FLOAT)m_blurTextureHeight;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;
	rect.right = m_blurTextureWidth;
	rect.bottom = m_blurTextureHeight;

	pContext->RSSetViewports(1, &viewport);
	pContext->RSSetScissorRects(1, &rect);

	pContext->RSSetState(m_pRasterizerState);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(nullptr);

	pContext->PSSetConstantBuffers(0, 1, &m_pBloomConstantBuffer);
	ID3D11SamplerState* samplers[] = { m_pMinMagMipPointSampler, m_pMinMagLinearSampler };
	pContext->PSSetSamplers(0, 2, samplers);

	RenderBloomMask(pHDRTextureSRV, pEmissiveSRV);

	Blur();

	AddBloom(pTargetTextureRTV);

	m_pContext->EndEvent();
}

void Bloom::RenderBloomMask(ID3D11ShaderResourceView* pHDRTextureSRV, ID3D11ShaderResourceView* pEmissiveSRV)
{
	m_pContext->BeginEvent(L"Bloom Mask");
	
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	pContext->OMSetRenderTargets(1, &m_pingPongBlurTexturesRTV[0], nullptr);

	pContext->VSSetShader(m_pBloomMaskVS, nullptr, 0);
	pContext->PSSetShader(m_pBloomMaskPS, nullptr, 0);

	ID3D11ShaderResourceView* SRVs[] = { pHDRTextureSRV, pEmissiveSRV };
	pContext->PSSetShaderResources(0, _countof(SRVs), SRVs);

	pContext->Draw(4, 0);

	pContext->OMSetRenderTargets(0, nullptr, nullptr);

	m_pContext->EndEvent();
}

void Bloom::Blur()
{
	m_pContext->BeginEvent(L"Gauss Blur");

	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	auto applyBlur = [&](UINT dstIdx, ID3D11VertexShader* pVS, ID3D11PixelShader* pPS)
	{
		UINT srcIdx = (dstIdx + 1) & 0x01u;

		pContext->PSSetShaderResources(0, 1, &m_pingPongBlurTexturesSRV[srcIdx]);
		pContext->OMSetRenderTargets(1, &m_pingPongBlurTexturesRTV[dstIdx], nullptr);

		pContext->VSSetShader(pVS, nullptr, 0);
		pContext->PSSetShader(pPS, nullptr, 0);

		pContext->Draw(4, 0);

		pContext->OMSetRenderTargets(0, nullptr, nullptr);
	};

	for (UINT i = 0; i < m_blurSteps; ++i)
	{
		applyBlur(1, m_pGaussBlurHorizontalVS, m_pGaussBlurHorizontalPS);
		applyBlur(0, m_pGaussBlurVerticalVS, m_pGaussBlurVerticalPS);
	}

	pContext->OMSetRenderTargets(0, nullptr, nullptr);

	m_pContext->EndEvent();
}

void Bloom::AddBloom(ID3D11RenderTargetView* pTargetRTV)
{
	m_pContext->BeginEvent(L"Add Bloom");

	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftY = 0;
	viewport.TopLeftX = 0;
	viewport.Width = (FLOAT)m_width;
	viewport.Height = (FLOAT)m_height;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;
	rect.right = m_width;
	rect.bottom = m_height;

	pContext->RSSetViewports(1, &viewport);
	pContext->RSSetScissorRects(1, &rect);

	pContext->OMSetRenderTargets(1, &pTargetRTV, nullptr);
	pContext->OMSetBlendState(m_pBlendState, nullptr, 0xffffffff);

	pContext->VSSetShader(m_pBloomVS, nullptr, 0);
	pContext->PSSetShader(m_pBloomPS, nullptr, 0);

	pContext->PSSetShaderResources(0, 1, &m_pingPongBlurTexturesSRV[0]);

	pContext->Draw(4, 0);

	m_pContext->EndEvent();
}
