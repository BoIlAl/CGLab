#include "HDRITextureLoader.h"
#include "common.h"


HDRITextureLoader* HDRITextureLoader::CreateHDRITextureLoader(RendererContext* pContext)
{
	HDRITextureLoader* pLoader = new HDRITextureLoader(pContext);

	if (pLoader->Init())
	{
		return pLoader;
	}

	delete pLoader;
	return nullptr;
}


HDRITextureLoader::HDRITextureLoader(RendererContext* pContext)
	: m_pContext(pContext)
	, m_pRasterizerState(nullptr)
	, m_pInputLayout(nullptr)
	, m_pVertexShader(nullptr)
	, m_pPixelShader(nullptr)
	, m_pTmpCubeEdge(nullptr)
	, m_pTmpCubeEdgeRTV(nullptr)
	, m_pSquareMesh(nullptr)
{}

HDRITextureLoader::~HDRITextureLoader()
{
	delete m_pSquareMesh;

	SafeRelease(m_pTmpCubeEdgeRTV);
	SafeRelease(m_pTmpCubeEdge);
	SafeRelease(m_pPixelShader);
	SafeRelease(m_pVertexShader);
	SafeRelease(m_pInputLayout);
	SafeRelease(m_pRasterizerState);
}


bool HDRITextureLoader::Init()
{
	HRESULT hr = CreatePipelineStateObjects();

	if (SUCCEEDED(hr))
	{

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
			&m_pVertexShader,
			&pBlob,
			&m_pPixelShader
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

	return hr;
}

HRESULT HDRITextureLoader::CreateResources()
{
	return S_OK;
}
