#include "toneMapping.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <DirectXMath.h>

#include "common.h"
#include "shaderCompiler.h"
#include "rendererContext.h"


struct ExposureBuffer
{
	DirectX::XMFLOAT4 exposure;
};

ToneMapping* ToneMapping::CreateToneMapping(RendererContext* pContext, UINT maxWindowSize)
{
	ToneMapping* pToneMapping = new ToneMapping(pContext, maxWindowSize);

	HRESULT hr = pToneMapping->CreatePipelineStateObjects();

	if (SUCCEEDED(hr))
	{
		hr = pToneMapping->CreateResources();
	}

	if (SUCCEEDED(hr))
	{
		return pToneMapping;
	}


	delete pToneMapping;
	return nullptr;
}


ToneMapping::ToneMapping(RendererContext* pContext, UINT maxWindowSize)
	: m_pContext(pContext)
	, m_pRasterizerState(nullptr)
	, m_pMinMagLinearSampler(nullptr)
	, m_pToneMappingVS(nullptr)
	, m_pToneMappingPS(nullptr)
	, m_adaptedBrightness(1.0f)
	, m_textureSize(MinPower2(maxWindowSize, maxWindowSize))
	, m_mipsNum(GetPowerOf2(m_textureSize))
	, m_mostDetailedMip(0)
	, m_pExposureTexture(nullptr)
	, m_pExposureDstTexture(nullptr)
	, m_pAverageBrightnessVS(nullptr)
	, m_pAverageBrightnessPS(nullptr)
	, m_pDownSampleVS(nullptr)
	, m_pDownSamplePS(nullptr)
	, m_pExposureBuffer(nullptr)
{}

ToneMapping::~ToneMapping()
{
	SafeRelease(m_pExposureBuffer);
	SafeRelease(m_pToneMappingPS);
	SafeRelease(m_pToneMappingVS);
	SafeRelease(m_pMinMagLinearSampler);
	SafeRelease(m_pRasterizerState);
	SafeRelease(m_pExposureTexture);
	SafeRelease(m_pAverageBrightnessVS);
	SafeRelease(m_pAverageBrightnessPS);
	SafeRelease(m_pDownSampleVS);
	SafeRelease(m_pDownSamplePS);
	SafeRelease(m_pExposureDstTexture);

	for (auto& srv : m_exposureTextureSRVs)
	{
		SafeRelease(srv);
	}

	for (auto& rtv : m_exposureTextureRTVs)
	{
		SafeRelease(rtv);
	}
}


HRESULT ToneMapping::CreatePipelineStateObjects()
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
		ID3DBlob* pVSBlob = nullptr;

		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/toneMapping.hlsl",
			&m_pToneMappingVS,
			&pVSBlob,
			&m_pToneMappingPS
		))
		{
			hr = E_FAIL;
		}

		SafeRelease(pVSBlob);
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

		hr = m_pContext->GetDevice()->CreateSamplerState(&samplerDesc, &m_pMinMagLinearSampler);
	}

	if (SUCCEEDED(hr))
	{
		ID3DBlob* pVSBlob = nullptr;

		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/averageBrightness.hlsl",
			&m_pAverageBrightnessVS,
			&pVSBlob,
			&m_pAverageBrightnessPS
		))
		{
			hr = E_FAIL;
		}

		SafeRelease(pVSBlob);
	}

	if (SUCCEEDED(hr))
	{
		ID3DBlob* pVSBlob = nullptr;

		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/downSample.hlsl",
			&m_pDownSampleVS,
			&pVSBlob,
			&m_pDownSamplePS
		))
		{
			hr = E_FAIL;
		}

		SafeRelease(pVSBlob);
	}


	return hr;
}


HRESULT ToneMapping::CreateResources()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	D3D11_BUFFER_DESC constantBufferDesc = CreateDefaultBufferDesc(sizeof(ExposureBuffer), D3D11_BIND_CONSTANT_BUFFER);

	HRESULT hr = pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pExposureBuffer);

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC exposureDesc = {};
		exposureDesc.Format = DXGI_FORMAT_R32_FLOAT;
		exposureDesc.Width = m_textureSize;
		exposureDesc.Height = m_textureSize;
		exposureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		exposureDesc.Usage = D3D11_USAGE_DEFAULT;
		exposureDesc.CPUAccessFlags = 0;
		exposureDesc.ArraySize = 1;
		exposureDesc.MipLevels = m_mipsNum + 1;
		exposureDesc.SampleDesc.Count = 1;
		exposureDesc.SampleDesc.Quality = 0;

		hr = pDevice->CreateTexture2D(&exposureDesc, nullptr, &m_pExposureTexture);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC exposureDstDesc = {};
		exposureDstDesc.Format = DXGI_FORMAT_R32_FLOAT;
		exposureDstDesc.Width = 1;
		exposureDstDesc.Height = 1;
		exposureDstDesc.BindFlags = 0;
		exposureDstDesc.Usage = D3D11_USAGE_STAGING;
		exposureDstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		exposureDstDesc.ArraySize = 1;
		exposureDstDesc.MipLevels = 1;
		exposureDstDesc.SampleDesc.Count = 1;
		exposureDstDesc.SampleDesc.Quality = 0;

		hr = pDevice->CreateTexture2D(&exposureDstDesc, nullptr, &m_pExposureDstTexture);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		for (UINT i = 0, textureSize = m_textureSize; textureSize > 0; textureSize >>= 1, ++i)
		{
			srvDesc.Texture2D.MostDetailedMip = i;

			ID3D11ShaderResourceView* pSrv = nullptr;
			hr = pDevice->CreateShaderResourceView(m_pExposureTexture, &srvDesc, &pSrv);

			if (FAILED(hr))
			{
				break;
			}

			m_exposureTextureSRVs.push_back(pSrv);
		}
	}

	if (SUCCEEDED(hr))
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		for (UINT i = 0, textureSize = m_textureSize; textureSize > 0; textureSize >>= 1, ++i)
		{
			rtvDesc.Texture2D.MipSlice = i;

			ID3D11RenderTargetView* pRtv = nullptr;
			hr = pDevice->CreateRenderTargetView(m_pExposureTexture, &rtvDesc, &pRtv);

			if (FAILED(hr))
			{
				break;
			}

			m_exposureTextureRTVs.push_back(pRtv);
		}
	}

	return hr;
}


FLOAT ToneMapping::EyeAdaptation(FLOAT currentExposure, FLOAT deltaTime) const
{
	return m_adaptedBrightness + (currentExposure - m_adaptedBrightness) * (1 - expf(-deltaTime));
}


UINT ToneMapping::DefaineMostDetailedMip(UINT width, UINT height) const
{
	UINT textureSize = MinPower2(width, height);
	UINT pow2 = GetPowerOf2(textureSize);

	assert(pow2 <= m_mipsNum);

	return m_mipsNum - pow2;
}


void ToneMapping::Update(FLOAT averageBrightness, FLOAT deltaTime)
{
	m_adaptedBrightness = EyeAdaptation(averageBrightness, deltaTime);

	static ExposureBuffer exposureBuffer = {};
	exposureBuffer.exposure = { (1.03f - (2.0f / (2 + log10f(m_adaptedBrightness + 1)))) / averageBrightness, 0.0f, 0.0f, 0.0f };

	m_pContext->GetContext()->UpdateSubresource(m_pExposureBuffer, 0, nullptr, &exposureBuffer, 0, 0);
}

FLOAT ToneMapping::CalculateAverageBrightness(
	ID3D11ShaderResourceView* pSrcTextureSRV,
	UINT renderTargetWidth,
	UINT renderTargetHeight
	)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	m_pContext->BeginEvent(L"Average Brightness");

	UINT textureSize = MinPower2(renderTargetWidth, renderTargetHeight);

	pContext->ClearState();

	pContext->OMSetRenderTargets(1, &m_exposureTextureRTVs[m_mostDetailedMip], nullptr);

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftY = 0;
	viewport.TopLeftX = 0;
	viewport.Width = (FLOAT)textureSize;
	viewport.Height = (FLOAT)textureSize;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;
	rect.right = textureSize;
	rect.bottom = textureSize;

	pContext->RSSetViewports(1, &viewport);
	pContext->RSSetScissorRects(1, &rect);
	pContext->RSSetState(m_pRasterizerState);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(nullptr);

	pContext->VSSetShader(m_pAverageBrightnessVS, nullptr, 0);
	pContext->PSSetShader(m_pAverageBrightnessPS, nullptr, 0);

	pContext->PSSetSamplers(0, 1, &m_pMinMagLinearSampler);
	pContext->PSSetShaderResources(0, 1, &pSrcTextureSRV);

	pContext->Draw(4, 0);

	pContext->VSSetShader(m_pDownSampleVS, nullptr, 0);
	pContext->PSSetShader(m_pDownSamplePS, nullptr, 0);

	UINT i = m_mostDetailedMip;

	for (UINT n = textureSize >> 1; n > 0; n >>= 1, ++i)
	{
		viewport.Width = (FLOAT)n;
		viewport.Height = (FLOAT)n;

		rect.right = n;
		rect.bottom = n;

		pContext->OMSetRenderTargets(1, &m_exposureTextureRTVs[(size_t)i + 1], nullptr);

		pContext->RSSetViewports(1, &viewport);
		pContext->RSSetScissorRects(1, &rect);

		pContext->PSSetShaderResources(0, 1, &m_exposureTextureSRVs[i]);

		pContext->Draw(4, 0);
	}

	pContext->CopySubresourceRegion(m_pExposureDstTexture, 0, 0, 0, 0, m_pExposureTexture, i, nullptr);

	D3D11_MAPPED_SUBRESOURCE resourceDesc = {};
	auto hr = pContext->Map(m_pExposureDstTexture, 0, D3D11_MAP_READ, 0, &resourceDesc);
	FLOAT averageBrightness = *reinterpret_cast<FLOAT*>(resourceDesc.pData);

	pContext->Unmap(m_pExposureDstTexture, 0);

	m_pContext->EndEvent();

	return expf(averageBrightness) - 1.0f;
}


HRESULT ToneMapping::ToneMap(
	ID3D11ShaderResourceView* pSrcTextureSRV,
	ID3D11RenderTargetView* pDstTextureRTV,
	UINT renderTargetWidth,
	UINT renderTargetHeight,
	FLOAT deltaTime
)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	m_mostDetailedMip = DefaineMostDetailedMip(renderTargetWidth, renderTargetHeight);

	Update(CalculateAverageBrightness(pSrcTextureSRV, renderTargetWidth, renderTargetHeight), deltaTime);

	m_pContext->BeginEvent(L"Tone Mapping");

	pContext->ClearState();

	pContext->OMSetRenderTargets(1, &pDstTextureRTV, nullptr);

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

	pContext->VSSetShader(m_pToneMappingVS, nullptr, 0);
	pContext->PSSetShader(m_pToneMappingPS, nullptr, 0);

	pContext->PSSetSamplers(0, 1, &m_pMinMagLinearSampler);
	pContext->PSSetShaderResources(0, 1, &pSrcTextureSRV);
	pContext->PSSetConstantBuffers(0, 1, &m_pExposureBuffer);

	pContext->Draw(4, 0);

	m_pContext->EndEvent();

	return S_OK;
}
