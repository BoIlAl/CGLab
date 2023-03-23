#include "prefilteredColor.h"
#include <d3d11.h>

struct RoughnessBuffer
{
	DirectX::XMFLOAT4 roughness;
};

struct ConstantBuffer
{
	DirectX::XMFLOAT4X4 modelMatrix;
	DirectX::XMFLOAT4X4 vpMatrix;
};

PrefilteredColor::~PrefilteredColor()
{
	SafeRelease(m_pPrefilteredColorVS);
	SafeRelease(m_pPrefilteredColorPS);
	SafeRelease(m_pRasterizerState);
	SafeRelease(m_pMinMagLinearSampler);
	SafeRelease(m_pRoughnessBuffer);
	SafeRelease(m_pConstantBuffer);
	SafeRelease(m_pTextureCubeResource);
	SafeRelease(m_pTextureCubeRecourceSRV);
	SafeRelease(m_pTextureCubeTarget);
	for(auto& rtv : m_pTextureCubeTargets)
	{
		SafeRelease(rtv);
	}
}

PrefilteredColor::PrefilteredColor(RendererContext* pContext)
	: m_pContext(pContext)
	, m_pPrefilteredColorVS(nullptr)
	, m_pPrefilteredColorPS(nullptr)
	, m_pRasterizerState(nullptr)
	, m_pMinMagLinearSampler(nullptr)
	, m_pRoughnessBuffer(nullptr)
	, m_pConstantBuffer(nullptr)
	, m_pTextureCubeResource(nullptr)
	, m_pTextureCubeRecourceSRV(nullptr)
	, m_pTextureCubeTarget(nullptr)
{
	m_edgesModelMatrices[0] = DirectX::XMMatrixRotationY(PI / 2.0f);    // +X
	m_edgesModelMatrices[1] = DirectX::XMMatrixRotationY(-PI / 2.0f);    // -X
	m_edgesModelMatrices[2] = DirectX::XMMatrixRotationX(-PI / 2.0f);    // +Y
	m_edgesModelMatrices[3] = DirectX::XMMatrixRotationX(PI / 2.0f);    // -Y
	m_edgesModelMatrices[4] = DirectX::XMMatrixIdentity();                // +Z
	m_edgesModelMatrices[5] = DirectX::XMMatrixRotationY(PI);            // -Z

	m_edgesViewMatrices[0] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }
	);    // +X
	m_edgesViewMatrices[1] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }
	);    // -X
	m_edgesViewMatrices[2] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }
	);    // +Y
	m_edgesViewMatrices[3] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }
	);    // -Y
	m_edgesViewMatrices[4] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }
	);    // +Z
	m_edgesViewMatrices[5] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }
	);    // -Z

	static const float nearPlane = 0.5f;
	static const float farPlane = 1.5f;
	static const float fov = PI / 2.0f;
	static const float width = nearPlane / tanf(fov / 2.0f);
	static const float height = width;
	m_projMatrix = DirectX::XMMatrixPerspectiveLH(2 * width, 2 * height, nearPlane, farPlane);
}

PrefilteredColor* PrefilteredColor::Create(RendererContext* pContext)
{
	PrefilteredColor* pPrefilteredColor = new PrefilteredColor(pContext);
	if (pPrefilteredColor != nullptr)
	{
		HRESULT hr = pPrefilteredColor->CreatePipelineStateObjects();
		if (SUCCEEDED(hr))
		{
			hr = pPrefilteredColor->CreateResources();
		}
		if (SUCCEEDED(hr))
		{
			return pPrefilteredColor;
		}
		else
		{
			delete pPrefilteredColor;
			return nullptr;
		}
	}
	return nullptr;
}


HRESULT PrefilteredColor::CreatePipelineStateObjects()
{
	HRESULT hr = S_OK;
	ID3DBlob* pVSBlob = nullptr;

	if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
		"shaders/prefilteredColor.hlsl",
		&m_pPrefilteredColorVS,
		&pVSBlob,
		&m_pPrefilteredColorPS
	))
	{
		hr = E_FAIL;
	}

	SafeRelease(pVSBlob);

	if (SUCCEEDED(hr))
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

	if (SUCCEEDED(hr))
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = m_pContext->GetDevice()->CreateSamplerState(&samplerDesc, &m_pMinMagLinearSampler);
	}

	return hr;
}

HRESULT PrefilteredColor::CreateResources()
{
	const UINT nMip = (UINT)m_roughnessValues.size();
	ID3D11Device* pDevice = m_pContext->GetDevice();

	D3D11_BUFFER_DESC constantBufferDesc = CreateDefaultBufferDesc(sizeof(RoughnessBuffer), D3D11_BIND_CONSTANT_BUFFER);

	HRESULT hr = pDevice->CreateBuffer(&constantBufferDesc, nullptr, &m_pRoughnessBuffer);
	

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC constBufferDesc = CreateDefaultBufferDesc(sizeof(ConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

		HRESULT hr = pDevice->CreateBuffer(&constBufferDesc, nullptr, &m_pConstantBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC TextureCubeResourceDesc = {};
		TextureCubeResourceDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		TextureCubeResourceDesc.Width = 512u;//
		TextureCubeResourceDesc.Height = 512u;////сделать нормально
		TextureCubeResourceDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		TextureCubeResourceDesc.Usage = D3D11_USAGE_DEFAULT;
		TextureCubeResourceDesc.CPUAccessFlags = 0;
		TextureCubeResourceDesc.ArraySize = 1;
		TextureCubeResourceDesc.MipLevels = nMip;
		TextureCubeResourceDesc.SampleDesc.Count = 1;
		TextureCubeResourceDesc.SampleDesc.Quality = 0;

		hr = pDevice->CreateTexture2D(&TextureCubeResourceDesc, nullptr, &m_pTextureCubeResource);
	}

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateShaderResourceView(m_pTextureCubeResource, nullptr, &m_pTextureCubeRecourceSRV);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC TextureCubeTargetDesc = {};
		TextureCubeTargetDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		TextureCubeTargetDesc.Width = m_prefilteredColorSize;   
		TextureCubeTargetDesc.Height = m_prefilteredColorSize;
		TextureCubeTargetDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		TextureCubeTargetDesc.Usage = D3D11_USAGE_DEFAULT;
		TextureCubeTargetDesc.CPUAccessFlags = 0;
		TextureCubeTargetDesc.ArraySize = 1;
		TextureCubeTargetDesc.MipLevels = nMip;
		TextureCubeTargetDesc.SampleDesc.Count = 1;
		TextureCubeTargetDesc.SampleDesc.Quality = 0;

		hr = pDevice->CreateTexture2D(&TextureCubeTargetDesc, nullptr, &m_pTextureCubeTarget);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		for (UINT i = 0; i < nMip; ++i)
		{
			rtvDesc.Texture2D.MipSlice = i;

			ID3D11RenderTargetView* pRtv = nullptr;
			hr = pDevice->CreateRenderTargetView(m_pTextureCubeTarget, &rtvDesc, &pRtv);

			if (FAILED(hr))
			{
				break;
			}

			m_pTextureCubeTargets.push_back(pRtv);
		}
	}

	return hr;
}


HRESULT PrefilteredColor::CalculatePrefilteredColor(
	ID3D11Texture2D* pEnviroment,
	ID3D11ShaderResourceView* pEnviromentSRV,
	ID3D11Texture2D** ppPrefilteredColor,
	ID3D11ShaderResourceView** ppPrefilteredColorSRV
)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();
	ID3D11Device* pDevice = m_pContext->GetDevice();

	const UINT nMip = (UINT)m_roughnessValues.size();

	m_pContext->BeginEvent(L"Prefiltered Color");

	D3D11_TEXTURE2D_DESC prefilteredColorDesc = {};
	prefilteredColorDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	prefilteredColorDesc.Width = m_prefilteredColorSize;
	prefilteredColorDesc.Height = m_prefilteredColorSize;
	prefilteredColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	prefilteredColorDesc.Usage = D3D11_USAGE_DEFAULT;
	prefilteredColorDesc.CPUAccessFlags = 0;
	prefilteredColorDesc.ArraySize = 6;
	prefilteredColorDesc.MipLevels = nMip;
	prefilteredColorDesc.SampleDesc.Count = 1;
	prefilteredColorDesc.SampleDesc.Quality = 0;
	prefilteredColorDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	HRESULT hr = pDevice->CreateTexture2D(&prefilteredColorDesc, nullptr, ppPrefilteredColor);

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateShaderResourceView(*ppPrefilteredColor, nullptr, ppPrefilteredColorSRV);
	}


	if (SUCCEEDED(hr))
	{

		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftY = 0;
		viewport.TopLeftX = 0;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;

		D3D11_RECT rect = {};
		rect.left = 0;
		rect.top = 0;

		pContext->RSSetState(m_pRasterizerState);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);

		pContext->VSSetShader(m_pPrefilteredColorVS, nullptr, 0);
		pContext->PSSetShader(m_pPrefilteredColorPS, nullptr, 0);
		pContext->PSSetSamplers(0, 1, &m_pMinMagLinearSampler);
		//pContext->PSSetShaderResources(0, 1, &m_pTextureCubeRecourceSRV);
		pContext->PSSetShaderResources(0, 1, &pEnviromentSRV);

		ID3D11Buffer* constantBuffers[] = { m_pConstantBuffer, m_pRoughnessBuffer };
		RoughnessBuffer roughnessBuffer = {};
		ConstantBuffer constBuffer = {};
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		for (UINT j = 0; j < 6; ++j)
		{
			
			for (UINT i = 0, size = m_prefilteredColorSize; i < nMip; ++i, size>>=1)
			{
				viewport.Width = (FLOAT)size;
				viewport.Height = (FLOAT)size;
				rect.right = size;
				rect.bottom = size;
				pContext->RSSetViewports(1, &viewport);
				pContext->RSSetScissorRects(1, &rect);

				pContext->OMSetRenderTargets(1, &m_pTextureCubeTargets[i], nullptr);

				DirectX::XMStoreFloat4x4(&constBuffer.modelMatrix, DirectX::XMMatrixTranspose(m_edgesModelMatrices[j]));
				DirectX::XMStoreFloat4x4(&constBuffer.vpMatrix, DirectX::XMMatrixTranspose(m_edgesViewMatrices[j] * m_projMatrix));

				pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constBuffer, 0, 0);

				DirectX::XMStoreFloat4(&roughnessBuffer.roughness, DirectX::FXMVECTOR({ m_roughnessValues[i], 0, 0, 0 }));
				pContext->UpdateSubresource(m_pRoughnessBuffer, 0, nullptr, &roughnessBuffer, 0, 0);

				pContext->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
				pContext->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
				pContext->Draw(4, 0);

				pContext->CopySubresourceRegion(*ppPrefilteredColor, D3D11CalcSubresource(i, j, nMip), 0, 0, 0, m_pTextureCubeTarget, D3D11CalcSubresource(i, 0, nMip), nullptr);
				pContext->ClearRenderTargetView(m_pTextureCubeTargets[i], clearColor);
			}
			
		}
	}

	m_pContext->EndEvent();

	return hr;
}