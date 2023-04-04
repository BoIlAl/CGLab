#include "HDRITextureLoader.h"
#include "common.h"
#include "stb_image.h"


struct ConstantBuffer
{
	DirectX::XMFLOAT4X4 modelMatrix;
	DirectX::XMFLOAT4X4 vpMatrix;
};

struct RoughnessBuffer
{
	DirectX::XMFLOAT4 roughness;
};


HDRITextureLoader* HDRITextureLoader::CreateHDRITextureLoader(
	RendererContext* pContext,
	UINT cubeTextureSize,
	UINT irradianceMapSize,
	UINT prefilteredTextureSize
)
{
	HDRITextureLoader* pLoader = new HDRITextureLoader(pContext, cubeTextureSize, irradianceMapSize, prefilteredTextureSize);

	if (pLoader->Init())
	{
		return pLoader;
	}

	delete pLoader;
	return nullptr;
}


HDRITextureLoader::HDRITextureLoader(
	RendererContext* pContext,
	UINT cubeTextureSize,
	UINT irradianceMapSize,
	UINT prefilteredTextureSize
)
	: m_pContext(pContext)
	, m_pRasterizerState(nullptr)
	, m_pInputLayout(nullptr)
	, m_pToCubeVS(nullptr)
	, m_pToCubePS(nullptr)
	, m_pIrradiancePS(nullptr)
	, m_pIrradianceVS(nullptr)
	, m_pPrefilteredVS(nullptr)
	, m_pPrefilteredPS(nullptr)
	, m_pMinMagLinearSampler(nullptr)
	, m_pTmpCubeEdge(nullptr)
	, m_pTmpCubeEdgeRTV(nullptr)
	, m_pConstantBuffer(nullptr)
	, m_pRoughnessBuffer(nullptr)
	, m_cubeTextureSize(cubeTextureSize)
	, m_irradianceMapSize(irradianceMapSize)
	, m_prefilteredTextureSize(prefilteredTextureSize)
	, m_rougnessValuesNum(5)
{
	m_edgesModelMatrices[0] = DirectX::XMMatrixRotationY(PI / 2.0f);	// +X
	m_edgesModelMatrices[1] = DirectX::XMMatrixRotationY(-PI / 2.0f);	// -X
	m_edgesModelMatrices[2] = DirectX::XMMatrixRotationX(-PI / 2.0f);	// +Y
	m_edgesModelMatrices[3] = DirectX::XMMatrixRotationX(PI / 2.0f);	// -Y
	m_edgesModelMatrices[4] = DirectX::XMMatrixIdentity();				// +Z
	m_edgesModelMatrices[5] = DirectX::XMMatrixRotationY(PI);			// -Z

	m_edgesViewMatrices[0] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f },	{ 0.0f, 1.0f, 0.0f, 0.0f }
	);	// +X
	m_edgesViewMatrices[1] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f, 0.0f },	{ 0.0f, 1.0f, 0.0f, 0.0f }
	);	// -X
	m_edgesViewMatrices[2] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }
	);	// +Y
	m_edgesViewMatrices[3] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }
	);	// -Y
	m_edgesViewMatrices[4] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }
	);	// +Z
	m_edgesViewMatrices[5] = DirectX::XMMatrixLookToLH(
		{ 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }
	);	// -Z

	static const float nearPlane = 0.5f;
	static const float farPlane = 1.5f;
	static const float fov = PI / 2.0f;
	static const float width = nearPlane / tanf(fov / 2.0f);
	static const float height = width;
	m_projMatrix = DirectX::XMMatrixPerspectiveLH(2 * width, 2 * height, nearPlane, farPlane);
}

HDRITextureLoader::~HDRITextureLoader()
{
	SafeRelease(m_pRoughnessBuffer);
	SafeRelease(m_pConstantBuffer);
	SafeRelease(m_pTmpCubeEdgeRTV);
	SafeRelease(m_pTmpCubeEdge);
	SafeRelease(m_pMinMagLinearSampler);
	SafeRelease(m_pPrefilteredPS);
	SafeRelease(m_pPrefilteredVS);
	SafeRelease(m_pIrradiancePS);
	SafeRelease(m_pIrradianceVS);
	SafeRelease(m_pToCubePS);
	SafeRelease(m_pToCubeVS);
	SafeRelease(m_pInputLayout);
	SafeRelease(m_pRasterizerState);
}


bool HDRITextureLoader::Init()
{
	HRESULT hr = CreatePipelineStateObjects();

	if (SUCCEEDED(hr))
	{
		hr = CreateResources();
	}

	return SUCCEEDED(hr);
}

HRESULT HDRITextureLoader::CreatePipelineStateObjects()
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
			"shaders/hdrToCube.hlsl",
			&m_pToCubeVS,
			&pBlob,
			&m_pToCubePS
		))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/irradianceMap.hlsl",
			&m_pIrradianceVS,
			&pBlob,
			&m_pIrradiancePS
		))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!m_pContext->GetShaderCompiler()->CreateVertexAndPixelShaders(
			"shaders/prefilteredColor.hlsl",
			&m_pPrefilteredVS,
			&pBlob,
			&m_pPrefilteredPS
		))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
			CreateInputElementDesc("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0)
		};

		hr = m_pContext->GetDevice()->CreateInputLayout(
			inputLayoutDesc,
			1,
			pBlob->GetBufferPointer(),
			pBlob->GetBufferSize(),
			&m_pInputLayout
		);
	}

	SafeRelease(pBlob);

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

HRESULT HDRITextureLoader::CreateResources()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	D3D11_BUFFER_DESC constBufferDesc = CreateDefaultBufferDesc(sizeof(ConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

	HRESULT hr = pDevice->CreateBuffer(&constBufferDesc, nullptr, &m_pConstantBuffer);

	if (SUCCEEDED(hr))
	{
		constBufferDesc = CreateDefaultBufferDesc(sizeof(RoughnessBuffer), D3D11_BIND_CONSTANT_BUFFER);

		hr = pDevice->CreateBuffer(&constBufferDesc, nullptr, &m_pRoughnessBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC cubeEdgeCopyDstDesc = CreateDefaultTexture2DDesc(
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			m_cubeTextureSize, m_cubeTextureSize,
			D3D11_BIND_RENDER_TARGET
		);

		hr = pDevice->CreateTexture2D(&cubeEdgeCopyDstDesc, nullptr, &m_pTmpCubeEdge);
	}

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRenderTargetView(m_pTmpCubeEdge, nullptr, &m_pTmpCubeEdgeRTV);
	}

	return hr;
}


HRESULT HDRITextureLoader::LoadTextureCubeFromHDRI(
	const std::string& fileName,
	ID3D11Texture2D** ppTextureCube,
	ID3D11ShaderResourceView** ppTextureCubeSRV
)
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	int width = 0;
	int height = 0;
	int n = 0;
	float* pImageData = stbi_loadf(fileName.c_str(), &width, &height, &n, 4);

	HRESULT hr = pImageData != nullptr ? S_OK : E_FAIL;

	ID3D11Texture2D* pHDRTextureSrc = nullptr;
	ID3D11ShaderResourceView* pHDRTextureSrcSRV = nullptr;

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC hdrTextureDesc = CreateDefaultTexture2DDesc(
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			width, height,
			D3D11_BIND_SHADER_RESOURCE
		);

		D3D11_SUBRESOURCE_DATA hdrTextureData = {};
		hdrTextureData.pSysMem = pImageData;
		hdrTextureData.SysMemPitch = 4u * width * sizeof(float);
		hdrTextureData.SysMemSlicePitch = 0;

		hr = pDevice->CreateTexture2D(&hdrTextureDesc, &hdrTextureData, &pHDRTextureSrc);
	}

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateShaderResourceView(pHDRTextureSrc, nullptr, &pHDRTextureSrcSRV);
	}


	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC cubeTextureDesc = CreateDefaultTexture2DDesc(
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			m_cubeTextureSize, m_cubeTextureSize,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET
		);
		cubeTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;
		cubeTextureDesc.ArraySize = 6;

		hr = pDevice->CreateTexture2D(&cubeTextureDesc, nullptr, ppTextureCube);
	}

	if (SUCCEEDED(hr))
	{
		Render(pHDRTextureSrcSRV, *ppTextureCube);

		if (ppTextureCubeSRV != nullptr)
		{
			hr = pDevice->CreateShaderResourceView(*ppTextureCube, nullptr, ppTextureCubeSRV);
		}
	}

	SafeRelease(pHDRTextureSrcSRV);
	SafeRelease(pHDRTextureSrc);

	delete pImageData;
	return hr;
}

HRESULT HDRITextureLoader::CalculateIrradianceMap(
	ID3D11ShaderResourceView* pTextureCubeSRV,
	ID3D11Texture2D** ppIrradianceMap,
	ID3D11ShaderResourceView** ppIrradianceMapSRV
)
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	D3D11_TEXTURE2D_DESC cubeTextureDesc = CreateDefaultTexture2DDesc(
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		m_irradianceMapSize, m_irradianceMapSize,
		D3D11_BIND_SHADER_RESOURCE
	);
	cubeTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	cubeTextureDesc.ArraySize = 6;

	HRESULT hr = pDevice->CreateTexture2D(&cubeTextureDesc, nullptr, ppIrradianceMap);

	if (SUCCEEDED(hr))
	{
		RenderIrradiance(pTextureCubeSRV, *ppIrradianceMap);

		if (ppIrradianceMapSRV != nullptr)
		{
			hr = pDevice->CreateShaderResourceView(*ppIrradianceMap, nullptr, ppIrradianceMapSRV);
		}
	}

	return hr;
}

HRESULT HDRITextureLoader::CalculatePrefilteredColor(
	ID3D11ShaderResourceView* pEnvironmentSRV,
	ID3D11Texture2D** ppPrefilteredColor,
	ID3D11ShaderResourceView** ppPrefilteredColorSRV
)
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	D3D11_TEXTURE2D_DESC prefilteredColorDesc = {};
	prefilteredColorDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	prefilteredColorDesc.Width = m_prefilteredTextureSize;
	prefilteredColorDesc.Height = m_prefilteredTextureSize;
	prefilteredColorDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	prefilteredColorDesc.Usage = D3D11_USAGE_DEFAULT;
	prefilteredColorDesc.CPUAccessFlags = 0;
	prefilteredColorDesc.ArraySize = 6;
	prefilteredColorDesc.MipLevels = m_rougnessValuesNum;
	prefilteredColorDesc.SampleDesc.Count = 1;
	prefilteredColorDesc.SampleDesc.Quality = 0;
	prefilteredColorDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	HRESULT hr = pDevice->CreateTexture2D(&prefilteredColorDesc, nullptr, ppPrefilteredColor);

	if (SUCCEEDED(hr))
	{
		RenderPrefiltered(pEnvironmentSRV, *ppPrefilteredColor);

		if (ppPrefilteredColorSRV != nullptr)
		{
			hr = pDevice->CreateShaderResourceView(*ppPrefilteredColor, nullptr, ppPrefilteredColorSRV);
		}
	}

	return hr;
}


void HDRITextureLoader::Render(ID3D11ShaderResourceView* pHDRTextureSrcSRV, ID3D11Texture2D* pTextureCube)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	m_pContext->BeginEvent(L"HDRI to CubeMap");

	pContext->ClearState();
	pContext->OMSetRenderTargets(1, &m_pTmpCubeEdgeRTV, nullptr);

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)m_cubeTextureSize;
	viewport.Height = (float)m_cubeTextureSize;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;
	rect.right = m_cubeTextureSize;
	rect.bottom = m_cubeTextureSize;

	pContext->RSSetViewports(1, &viewport);
	pContext->RSSetScissorRects(1, &rect);
	pContext->RSSetState(m_pRasterizerState);

	pContext->IASetInputLayout(nullptr);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pContext->VSSetShader(m_pToCubeVS, nullptr, 0);
	pContext->PSSetShader(m_pToCubePS, nullptr, 0);

	pContext->PSSetShaderResources(0, 1, &pHDRTextureSrcSRV);
	pContext->PSSetSamplers(0, 1, &m_pMinMagLinearSampler);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ConstantBuffer constBuffer = {};

	for (UINT i = 0; i < 6; ++i)
	{
		pContext->ClearRenderTargetView(m_pTmpCubeEdgeRTV, clearColor);

		DirectX::XMStoreFloat4x4(&constBuffer.modelMatrix, DirectX::XMMatrixTranspose(m_edgesModelMatrices[i]));
		DirectX::XMStoreFloat4x4(&constBuffer.vpMatrix, DirectX::XMMatrixTranspose(m_edgesViewMatrices[i] * m_projMatrix));

		pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constBuffer, 0, 0);

		pContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		pContext->Draw(4, 0);

		pContext->CopySubresourceRegion(pTextureCube, i, 0, 0, 0, m_pTmpCubeEdge, 0, nullptr);
	}
	
	m_pContext->EndEvent();
}

void HDRITextureLoader::RenderIrradiance(ID3D11ShaderResourceView* pEnvironmentTextureSRV, ID3D11Texture2D* pIrradianceCube)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	m_pContext->BeginEvent(L"Irradiance");

	pContext->ClearState();
	pContext->OMSetRenderTargets(1, &m_pTmpCubeEdgeRTV, nullptr);

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)m_irradianceMapSize;
	viewport.Height = (float)m_irradianceMapSize;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;
	rect.right = m_irradianceMapSize;
	rect.bottom = m_irradianceMapSize;

	pContext->RSSetViewports(1, &viewport);
	pContext->RSSetScissorRects(1, &rect);
	pContext->RSSetState(m_pRasterizerState);

	pContext->IASetInputLayout(nullptr);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pContext->VSSetShader(m_pIrradianceVS, nullptr, 0);
	pContext->PSSetShader(m_pIrradiancePS, nullptr, 0);

	pContext->PSSetShaderResources(0, 1, &pEnvironmentTextureSRV);
	pContext->PSSetSamplers(0, 1, &m_pMinMagLinearSampler);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ConstantBuffer constBuffer = {};

	for (UINT i = 0; i < 6; ++i)
	{
		pContext->ClearRenderTargetView(m_pTmpCubeEdgeRTV, clearColor);

		DirectX::XMStoreFloat4x4(&constBuffer.modelMatrix, DirectX::XMMatrixTranspose(m_edgesModelMatrices[i]));
		DirectX::XMStoreFloat4x4(&constBuffer.vpMatrix, DirectX::XMMatrixTranspose(m_edgesViewMatrices[i] * m_projMatrix));

		pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constBuffer, 0, 0);

		pContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		pContext->Draw(4, 0);

		D3D11_BOX srcBox = {};
		srcBox.left = 0;
		srcBox.top = 0;
		srcBox.front = 0;
		srcBox.right = m_irradianceMapSize;
		srcBox.bottom = m_irradianceMapSize;
		srcBox.back = 1;

		pContext->CopySubresourceRegion(pIrradianceCube, i, 0, 0, 0, m_pTmpCubeEdge, 0, &srcBox);
	}

	m_pContext->EndEvent();
}

void HDRITextureLoader::RenderPrefiltered(ID3D11ShaderResourceView* pEnvironmentTextureSRV, ID3D11Texture2D* pPrefilteredCube)
{
	ID3D11DeviceContext* pContext = m_pContext->GetContext();

	m_pContext->BeginEvent(L"Prefiltered Color");

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftY = 0;
	viewport.TopLeftX = 0;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	D3D11_RECT rect = {};
	rect.left = 0;
	rect.top = 0;

	D3D11_BOX srcBox = {};
	srcBox.left = 0;
	srcBox.top = 0;
	srcBox.front = 0;
	srcBox.right = 1;
	srcBox.bottom = 1;
	srcBox.back = 1;

	pContext->ClearState();
	pContext->OMSetRenderTargets(1, &m_pTmpCubeEdgeRTV, nullptr);

	pContext->RSSetState(m_pRasterizerState);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(nullptr);

	pContext->VSSetShader(m_pPrefilteredVS, nullptr, 0);
	pContext->PSSetShader(m_pPrefilteredPS, nullptr, 0);
	pContext->PSSetSamplers(0, 1, &m_pMinMagLinearSampler);
	pContext->PSSetShaderResources(0, 1, &pEnvironmentTextureSRV);

	ID3D11Buffer* constantBuffers[] = { m_pConstantBuffer, m_pRoughnessBuffer };
	RoughnessBuffer roughnessBuffer = {};
	ConstantBuffer constBuffer = {};

	pContext->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
	pContext->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);

	for (UINT j = 0; j < 6; ++j)
	{
		DirectX::XMStoreFloat4x4(&constBuffer.modelMatrix, DirectX::XMMatrixTranspose(m_edgesModelMatrices[j]));
		DirectX::XMStoreFloat4x4(&constBuffer.vpMatrix, DirectX::XMMatrixTranspose(m_edgesViewMatrices[j] * m_projMatrix));

		pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constBuffer, 0, 0);

		for (UINT i = 0, size = m_prefilteredTextureSize; i < m_rougnessValuesNum; ++i, size >>= 1)
		{
			viewport.Width = (FLOAT)size;
			viewport.Height = (FLOAT)size;
			rect.right = size;
			rect.bottom = size;

			pContext->RSSetViewports(1, &viewport);
			pContext->RSSetScissorRects(1, &rect);

			DirectX::XMStoreFloat4(&roughnessBuffer.roughness, DirectX::FXMVECTOR({ (float)i / (m_rougnessValuesNum - 1), 0, 0, 0 }));
			pContext->UpdateSubresource(m_pRoughnessBuffer, 0, nullptr, &roughnessBuffer, 0, 0);

			pContext->Draw(4, 0);

			srcBox.right = size;
			srcBox.bottom = size;

			pContext->CopySubresourceRegion(
				pPrefilteredCube,
				D3D11CalcSubresource(i, j, m_rougnessValuesNum),
				0, 0, 0,
				m_pTmpCubeEdge,
				0,
				&srcBox
			);
		}
	}

	m_pContext->EndEvent();
}
