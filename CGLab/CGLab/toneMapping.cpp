#include "toneMapping.h"

#include <d3d11.h>
#include <DirectXMath.h>

#include "common.h"
#include "shaderCompiler.h"


struct ExposureBuffer
{
	DirectX::XMFLOAT4 exposure;
};



ToneMapping* ToneMapping::CreateToneMapping(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ShaderCompiler* pShaderCompiler)
{
	ToneMapping* pToneMapping = new ToneMapping(pDevice, pContext);

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


ToneMapping::ToneMapping(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	: m_pDevice(pDevice)
	, m_pContext(pContext)
	, m_pRasterizerState(nullptr)
	, m_pMinMagLinearSampler(nullptr)
	, m_pToneMappingVS(nullptr)
	, m_pToneMappingPS(nullptr)
	, m_pExposureBuffer(nullptr)
	, m_adaptedExposure(0.0f)
{}

ToneMapping::~ToneMapping()
{
	SafeRelease(m_pExposureBuffer);
	SafeRelease(m_pToneMappingPS);
	SafeRelease(m_pToneMappingVS);
	SafeRelease(m_pMinMagLinearSampler);
	SafeRelease(m_pRasterizerState);
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

	return hr;
}


HRESULT ToneMapping::CreateResources()
{
	D3D11_BUFFER_DESC constantBufferDesc = CreateDefaultBufferDesc(sizeof(ExposureBuffer), D3D11_BIND_CONSTANT_BUFFER);

	return m_pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pExposureBuffer);
}


FLOAT ToneMapping::EyeAdaptation(FLOAT currentExposure, FLOAT deltaTime) const
{
	return m_adaptedExposure + (currentExposure - m_adaptedExposure) * (1 - expf(-deltaTime));
}


void ToneMapping::Update(FLOAT currentExposure, FLOAT deltaTime)
{
	m_adaptedExposure = EyeAdaptation(1.0f, deltaTime);

	static ExposureBuffer exposureBuffer = {};
	exposureBuffer.exposure = { m_adaptedExposure, 0.0f, 0.0f, 0.0f };

	m_pContext->UpdateSubresource(m_pExposureBuffer, 0, nullptr, &exposureBuffer, 0, 0);
}


HRESULT ToneMapping::ToneMap(
	ID3D11ShaderResourceView* pSrcTextureSRV,
	ID3D11RenderTargetView* pDstTextureRTV,
	UINT renderTargetWidth,
	UINT renderTargetHeight,
	FLOAT deltaTime
)
{
	Update(1.0f, deltaTime);

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
