#include "toneMapping.h"

#include <d3d11.h>
#include <DirectXMath.h>

#include "common.h"
#include "shaderCompiler.h"


struct ExposureBuffer
{
	DirectX::XMFLOAT4 exposure;
};

ToneMapping* ToneMapping::CreateToneMapping(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, 
											ShaderCompiler* pShaderCompiler, UINT windowWidth, UINT windowHeight)
{
	ToneMapping* pToneMapping = new ToneMapping(pDevice, pContext, windowWidth, windowHeight);

	HRESULT hr = pToneMapping->CreatePipelineStateObjects(pShaderCompiler);

	if (SUCCEEDED(hr))
	{
		pToneMapping->CreateResources();
	}

	if (SUCCEEDED(hr))
	{
		return pToneMapping;
	}


	delete pToneMapping;
	return nullptr;
}


ToneMapping::ToneMapping(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, UINT windowWidth, UINT windowHeight)
	: m_pDevice(pDevice)
	, m_pContext(pContext)
	, m_pRasterizerState(nullptr)
	, m_pMinMagLinearSampler(nullptr)
	, m_pToneMappingVS(nullptr)
	, m_pToneMappingPS(nullptr)
	, m_pExposureBuffer(nullptr)
	, m_adaptedExposure(1.0f)
	, m_textureSize(MinPower2(windowWidth, windowHeight))
	, m_pExposureTexture(nullptr)
	, m_pAverageBrightnessVS(nullptr)
	, m_pAverageBrightnessPS(nullptr)
	, m_pDownSampleVS(nullptr)
	, m_pDownSamplePS(nullptr)
	, m_pExposureDstTexture(nullptr)
	, m_pExposureTextureSRV(nullptr)
	, m_pExposureTextureRTV(nullptr)
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
	SafeRelease(m_pExposureTextureSRV);
	SafeRelease(m_pExposureTextureRTV);
}


HRESULT ToneMapping::CreatePipelineStateObjects(ShaderCompiler* pShaderCompiler)
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
		ID3DBlob* pVSBlob = nullptr;

		if (!pShaderCompiler->CreateVertexAndPixelShaders(
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

		hr = m_pDevice->CreateSamplerState(&samplerDesc, &m_pMinMagLinearSampler);
	}

	if (SUCCEEDED(hr))
	{
		ID3DBlob* pVSBlob = nullptr;

		if (!pShaderCompiler->CreateVertexAndPixelShaders(
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

		if (!pShaderCompiler->CreateVertexAndPixelShaders(
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
	D3D11_BUFFER_DESC constantBufferDesc = CreateDefaultBufferDesc(sizeof(ExposureBuffer), D3D11_BIND_CONSTANT_BUFFER);

	HRESULT hr = m_pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pExposureBuffer);

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
		exposureDesc.MipLevels = 0;
		exposureDesc.SampleDesc.Count = 1;
		exposureDesc.SampleDesc.Quality = 0;

		hr = m_pDevice->CreateTexture2D(&exposureDesc, nullptr, &m_pExposureTexture);
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
		exposureDstDesc.MipLevels = 0;
		exposureDstDesc.SampleDesc.Count = 1;
		exposureDstDesc.SampleDesc.Quality = 0;

		hr = m_pDevice->CreateTexture2D(&exposureDstDesc, nullptr, &m_pExposureDstTexture);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		hr = m_pDevice->CreateShaderResourceView(m_pExposureTexture, &srvDesc, &m_pExposureTextureSRV);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		hr = m_pDevice->CreateRenderTargetView(m_pExposureTexture, &rtvDesc, &m_pExposureTextureRTV);
	}

	return hr;
}


FLOAT ToneMapping::EyeAdaptation(FLOAT currentExposure, FLOAT deltaTime) const
{
	return m_adaptedExposure + (currentExposure - m_adaptedExposure) * (1 - expf(-deltaTime));
}


void ToneMapping::Update(FLOAT averageBrightness, FLOAT deltaTime)
{
	m_adaptedExposure = EyeAdaptation(averageBrightness, deltaTime);

	static ExposureBuffer exposureBuffer = {};
	exposureBuffer.exposure = { (1.03f - (2.0f / (2 + log10f(m_adaptedExposure + 1)))) / averageBrightness, 0.0f, 0.0f, 0.0f };

	m_pContext->UpdateSubresource(m_pExposureBuffer, 0, nullptr, &exposureBuffer, 0, 0);
}

float ToneMapping::CalculateAverageBrightness(
	ID3D11ShaderResourceView* pSrcTextureSRV,
	UINT renderTargetWidth,
	UINT renderTargetHeight
	)
{
	m_pContext->ClearState();

	m_pContext->OMSetRenderTargets(1, &m_pExposureTextureRTV, nullptr);

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftY = 0;
	viewport.TopLeftX = 0;
	viewport.Width = (FLOAT)m_textureSize;
	viewport.Height = (FLOAT)m_textureSize;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;
	rect.right = m_textureSize;
	rect.bottom = m_textureSize;

	m_pContext->RSSetViewports(1, &viewport);
	m_pContext->RSSetScissorRects(1, &rect);
	m_pContext->RSSetState(m_pRasterizerState);

	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pContext->IASetInputLayout(nullptr);

	m_pContext->VSSetShader(m_pAverageBrightnessVS, nullptr, 0);
	m_pContext->PSSetShader(m_pAverageBrightnessPS, nullptr, 0);

	m_pContext->PSSetSamplers(0, 1, &m_pMinMagLinearSampler);
	m_pContext->PSSetShaderResources(0, 1, &pSrcTextureSRV);

	m_pContext->Draw(4, 0);

	m_pContext->VSSetShader(m_pDownSampleVS, nullptr, 0);
	m_pContext->PSSetShader(m_pDownSamplePS, nullptr, 0);

	UINT i = 0;

	for (UINT n = m_textureSize >> 1; n > 0; n >>= 1, ++i)
	{
		viewport.Width = (FLOAT)n;
		viewport.Height = (FLOAT)n;

		rect.right = n;
		rect.bottom = n;

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = i + 1;

		ID3D11RenderTargetView* pRtv;
		m_pDevice->CreateRenderTargetView(m_pExposureTexture, &rtvDesc, &pRtv);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = i;

		ID3D11ShaderResourceView* pSrv;
		m_pDevice->CreateShaderResourceView(m_pExposureTexture, &srvDesc, &pSrv);

		m_pContext->OMSetRenderTargets(1, &pRtv, nullptr);

		m_pContext->RSSetViewports(1, &viewport);
		m_pContext->RSSetScissorRects(1, &rect);

		m_pContext->PSSetShaderResources(0, 1, &pSrv);

		m_pContext->Draw(4, 0);

		SafeRelease(pSrv);
		SafeRelease(pRtv);
	}

	m_pContext->CopySubresourceRegion(m_pExposureDstTexture, 0, 0, 0, 0, m_pExposureTexture, i, nullptr);

	D3D11_MAPPED_SUBRESOURCE resourceDesc = {};
	auto hr = m_pContext->Map(m_pExposureDstTexture, 0, D3D11_MAP_READ, 0, &resourceDesc);
	FLOAT averageBrightness = *reinterpret_cast<FLOAT*>(resourceDesc.pData);

	m_pContext->Unmap(m_pExposureDstTexture, 0);

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
	Update(CalculateAverageBrightness(pSrcTextureSRV, renderTargetWidth, renderTargetHeight), deltaTime);

	m_pContext->ClearState();

	m_pContext->OMSetRenderTargets(1, &pDstTextureRTV, nullptr);

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

	m_pContext->RSSetViewports(1, &viewport);
	m_pContext->RSSetScissorRects(1, &rect);
	m_pContext->RSSetState(m_pRasterizerState);

	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pContext->IASetInputLayout(nullptr);

	m_pContext->VSSetShader(m_pToneMappingVS, nullptr, 0);
	m_pContext->PSSetShader(m_pToneMappingPS, nullptr, 0);

	m_pContext->PSSetSamplers(0, 1, &m_pMinMagLinearSampler);
	m_pContext->PSSetShaderResources(0, 1, &pSrcTextureSRV);
	m_pContext->PSSetConstantBuffers(0, 1, &m_pExposureBuffer);

	m_pContext->Draw(4, 0);

	return S_OK;
}
