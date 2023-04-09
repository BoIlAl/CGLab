#include "bloom.h"

struct BrightnessThresholdBuffer
{
	DirectX::XMFLOAT4 brightnessThreshold;
};

struct TextureSizeBuffer
{
	DirectX::XMFLOAT4 textureSize;
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
	SafeRelease(m_pTextureSizeBuffer);
	SafeRelease(m_pRasterizerState);
	SafeRelease(m_pMinMagMipPointSampler);
	SafeRelease(m_pBloomMaskPS);
	SafeRelease(m_pBloomMaskVS);
	SafeRelease(m_pGaussBlurVerticalPS);
	SafeRelease(m_pGaussBlurVerticalVS);
	SafeRelease(m_pGaussBlurHorizontalPS);
	SafeRelease(m_pGaussBlurHorizontalVS);
	SafeRelease(m_pBloomPS);
	SafeRelease(m_pBloomVS);
}

Bloom::Bloom(RendererContext* pContext)
	: m_pContext(pContext)
	, m_pBrightnessThresholdBuffer(nullptr)
	, m_pTextureSizeBuffer(nullptr)
	, m_pRasterizerState(nullptr)
	, m_pMinMagMipPointSampler(nullptr)
	, m_pBloomMaskPS(nullptr)
	, m_pBloomMaskVS(nullptr)
	, m_pGaussBlurVerticalPS(nullptr)
	, m_pGaussBlurVerticalVS(nullptr)
	, m_pGaussBlurHorizontalPS(nullptr)
	, m_pGaussBlurHorizontalVS(nullptr)
	, m_pBloomPS(nullptr)
	, m_pBloomVS(nullptr)
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

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/gaussBlur.hlsl",
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
			"shaders/gaussBlur.hlsl",
			&m_pGaussBlurHorizontalVS,
			&pBlob,
			&m_pGaussBlurHorizontalPS
		))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/sumShader.hlsl",
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
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = m_pContext->GetDevice()->CreateSamplerState(&samplerDesc, &m_pMinMagMipPointSampler);
	}


	return hr;
}

HRESULT Bloom::CreateResources()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	D3D11_BUFFER_DESC brighnessThresholdBufferDesc = CreateDefaultBufferDesc(sizeof(BrightnessThresholdBuffer), D3D11_BIND_CONSTANT_BUFFER);

	auto brightnessThreshold = DirectX::FXMVECTOR({ m_brightnessThreshold, 0, 0, 0 });

	D3D11_SUBRESOURCE_DATA constBufferData = CreateDefaultSubresourceData(&brightnessThreshold);

	HRESULT hr = pDevice->CreateBuffer(&brighnessThresholdBufferDesc, &constBufferData, &m_pBrightnessThresholdBuffer);

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC textureSizeDesc = CreateDefaultBufferDesc(sizeof(TextureSizeBuffer), D3D11_BIND_CONSTANT_BUFFER);
		hr = pDevice->CreateBuffer(&textureSizeDesc, nullptr, &m_pTextureSizeBuffer);
	}

	return hr;
}

HRESULT Bloom::CalculateBloom(
	ID3D11ShaderResourceView* pHDRTextureSRV,
	ID3D11ShaderResourceView* pEmissiveSRV,
	UINT renderTargetWidth,
	UINT renderTargetHeight,
	ID3D11Texture2D** ppBloom,
	ID3D11ShaderResourceView** ppBloomSRV
)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();
	ID3D11Device* pDevice = m_pContext->GetDevice();
	ID3D11Texture2D* pBloomMask;
	ID3D11ShaderResourceView* pBloomMaskSRV;

	HRESULT hr = CalculateBloomMask(
		pHDRTextureSRV,
		pEmissiveSRV,
		renderTargetWidth,
		renderTargetHeight,
		&pBloomMask,
		&pBloomMaskSRV
	);

	if (SUCCEEDED(hr))
	{
		m_pContext->BeginEvent(L"Blur");
		for (size_t i = 0; i < m_blurSteps; ++i)
		{
			ID3D11Texture2D* pBlurResult = nullptr;
			ID3D11ShaderResourceView* pBlurResultSRV = nullptr;
			if (SUCCEEDED(hr))
			{
				hr = GaussBlurVertical(
					pBloomMaskSRV,
					renderTargetWidth,
					renderTargetHeight,
					&pBlurResult,
					&pBlurResultSRV
				);
			}
			SafeRelease(pBloomMask);
			SafeRelease(pBloomMaskSRV);
			pBloomMask = pBlurResult;
			pBloomMaskSRV = pBlurResultSRV;

			if (SUCCEEDED(hr))
			{
				hr = GaussBlurHorizontal(
					pBloomMaskSRV,
					renderTargetWidth,
					renderTargetHeight,
					&pBlurResult,
					&pBlurResultSRV
				);
			}

			SafeRelease(pBloomMask);
			SafeRelease(pBloomMaskSRV);
			pBloomMask = pBlurResult;
			pBloomMaskSRV = pBlurResultSRV;
		}
		m_pContext->EndEvent();
	}


	ID3D11RenderTargetView* pBloomRTV = nullptr;

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC bloomDesc = {};
		bloomDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		bloomDesc.Width = renderTargetWidth;
		bloomDesc.Height = renderTargetHeight;
		bloomDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		bloomDesc.Usage = D3D11_USAGE_DEFAULT;
		bloomDesc.CPUAccessFlags = 0;
		bloomDesc.ArraySize = 1;
		bloomDesc.MipLevels = 1;
		bloomDesc.SampleDesc.Count = 1;
		bloomDesc.SampleDesc.Quality = 0;


		hr = pDevice->CreateTexture2D(&bloomDesc, nullptr, ppBloom);
	}
	

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRenderTargetView(*ppBloom, nullptr, &pBloomRTV);
	}

	if (SUCCEEDED(hr))
	{
		RenderBloom(
			pHDRTextureSRV,
			pBloomMaskSRV,
			pBloomRTV,
			renderTargetWidth,
			renderTargetHeight
		);

		if (ppBloomSRV != nullptr)
		{
			hr = pDevice->CreateShaderResourceView(*ppBloom, nullptr, ppBloomSRV);
		}
	}

	SafeRelease(pBloomRTV);
	SafeRelease(pBloomMask);
	SafeRelease(pBloomMaskSRV);

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
	bloomMaskDesc.MipLevels = 1;
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

void Bloom::RenderBloom(
	ID3D11ShaderResourceView* pHDRTextureSRV,
	ID3D11ShaderResourceView* pBloomMaskSRV,
	ID3D11RenderTargetView* pBloomRTV,
	UINT renderTargetWidth,
	UINT renderTargetHeight
)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();
	m_pContext->BeginEvent(L"Bloom");

	pContext->OMSetRenderTargets(1, &pBloomRTV, nullptr);

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

	pContext->VSSetShader(m_pBloomVS, nullptr, 0);
	pContext->PSSetShader(m_pBloomPS, nullptr, 0);

	ID3D11ShaderResourceView* SRVs[] = { pHDRTextureSRV, pBloomMaskSRV };

	pContext->PSSetShaderResources(0, _countof(SRVs), SRVs);

	pContext->Draw(4, 0);

	m_pContext->EndEvent();
}

HRESULT Bloom::GaussBlurVertical(
	ID3D11ShaderResourceView* pBloomMaskSRV,
	UINT renderTargetWidth,
	UINT renderTargetHeight,
	ID3D11Texture2D** ppBlurResult,
	ID3D11ShaderResourceView** ppBlurResultSRV
)
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	ID3D11RenderTargetView* pBlurResultRTV = nullptr;

	D3D11_TEXTURE2D_DESC bloomMaskDesc = {};
	bloomMaskDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	bloomMaskDesc.Width = renderTargetWidth;
	bloomMaskDesc.Height = renderTargetHeight;
	bloomMaskDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	bloomMaskDesc.Usage = D3D11_USAGE_DEFAULT;
	bloomMaskDesc.CPUAccessFlags = 0;
	bloomMaskDesc.ArraySize = 1;
	bloomMaskDesc.MipLevels = 1;
	bloomMaskDesc.SampleDesc.Count = 1;
	bloomMaskDesc.SampleDesc.Quality = 0;


	HRESULT hr = pDevice->CreateTexture2D(&bloomMaskDesc, nullptr, ppBlurResult); //

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRenderTargetView(*ppBlurResult, nullptr, &pBlurResultRTV);
	}

	if (SUCCEEDED(hr))
	{
		RenderBlurVertical(
			pBloomMaskSRV,
			pBlurResultRTV,
			renderTargetWidth,
			renderTargetHeight
		);

		if (ppBlurResultSRV != nullptr)
		{
			hr = pDevice->CreateShaderResourceView(*ppBlurResult, nullptr, ppBlurResultSRV);
		}
	}

	SafeRelease(pBlurResultRTV);
	return hr;
}

void Bloom::RenderBlurVertical(
	ID3D11ShaderResourceView* pBloomMaskSRV,
	ID3D11RenderTargetView* pBlurResultRTV,
	UINT renderTargetWidth,
	UINT renderTargetHeight
)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();
	m_pContext->BeginEvent(L"Blur Vertical");

	pContext->OMSetRenderTargets(1, &pBlurResultRTV, nullptr);

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

	pContext->PSSetSamplers(0, 1, &m_pMinMagMipPointSampler);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(nullptr);

	pContext->VSSetShader(m_pGaussBlurVerticalVS, nullptr, 0);
	pContext->PSSetShader(m_pGaussBlurVerticalPS, nullptr, 0);

	pContext->PSSetShaderResources(0, 1, &pBloomMaskSRV);

	TextureSizeBuffer textureSizeBuffer = {};
	DirectX::XMStoreFloat4(&textureSizeBuffer.textureSize, DirectX::FXMVECTOR({(FLOAT)renderTargetWidth, (FLOAT)renderTargetHeight, 0, 0 }));
	pContext->UpdateSubresource(m_pTextureSizeBuffer, 0, nullptr, &textureSizeBuffer, 0, 0);


	pContext->PSSetConstantBuffers(0, 1, &m_pTextureSizeBuffer);

	pContext->Draw(4, 0);

	m_pContext->EndEvent();
}


HRESULT Bloom::GaussBlurHorizontal(
	ID3D11ShaderResourceView* pBloomMaskSRV,
	UINT renderTargetWidth,
	UINT renderTargetHeight,
	ID3D11Texture2D** ppBlurResult,
	ID3D11ShaderResourceView** ppBlurResultSRV
)
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	ID3D11RenderTargetView* pBlurResultRTV = nullptr;

	D3D11_TEXTURE2D_DESC bloomMaskDesc = {};
	bloomMaskDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	bloomMaskDesc.Width = renderTargetWidth;
	bloomMaskDesc.Height = renderTargetHeight;
	bloomMaskDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	bloomMaskDesc.Usage = D3D11_USAGE_DEFAULT;
	bloomMaskDesc.CPUAccessFlags = 0;
	bloomMaskDesc.ArraySize = 1;
	bloomMaskDesc.MipLevels = 1;
	bloomMaskDesc.SampleDesc.Count = 1;
	bloomMaskDesc.SampleDesc.Quality = 0;


	HRESULT hr = pDevice->CreateTexture2D(&bloomMaskDesc, nullptr, ppBlurResult); //

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRenderTargetView(*ppBlurResult, nullptr, &pBlurResultRTV);
	}

	if (SUCCEEDED(hr))
	{
		RenderBlurHorizontal(
			pBloomMaskSRV,
			pBlurResultRTV,
			renderTargetWidth,
			renderTargetHeight
		);

		if (ppBlurResultSRV != nullptr)
		{
			hr = pDevice->CreateShaderResourceView(*ppBlurResult, nullptr, ppBlurResultSRV);
		}
	}

	SafeRelease(pBlurResultRTV);
	return hr;
}

void Bloom::RenderBlurHorizontal(
	ID3D11ShaderResourceView* pBloomMaskSRV,
	ID3D11RenderTargetView* pBlurResultRTV,
	UINT renderTargetWidth,
	UINT renderTargetHeight
)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();
	m_pContext->BeginEvent(L"Blur Horizontal");

	pContext->OMSetRenderTargets(1, &pBlurResultRTV, nullptr);

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

	pContext->PSSetSamplers(0, 1, &m_pMinMagMipPointSampler);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(nullptr);

	pContext->VSSetShader(m_pGaussBlurHorizontalVS, nullptr, 0);
	pContext->PSSetShader(m_pGaussBlurHorizontalPS, nullptr, 0);

	pContext->PSSetShaderResources(0, 1, &pBloomMaskSRV);

	TextureSizeBuffer textureSizeBuffer = {};
	DirectX::XMStoreFloat4(&textureSizeBuffer.textureSize, DirectX::FXMVECTOR({ (FLOAT)renderTargetWidth, (FLOAT)renderTargetHeight, 0, 0 }));
	pContext->UpdateSubresource(m_pTextureSizeBuffer, 0, nullptr, &textureSizeBuffer, 0, 0);


	pContext->PSSetConstantBuffers(0, 1, &m_pTextureSizeBuffer);

	pContext->Draw(4, 0);

	m_pContext->EndEvent();
}

