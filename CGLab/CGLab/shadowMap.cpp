#include "shadowMap.h"
#include "rendererContext.h"


ShadowMap* ShadowMap::CreateShadowMap(RendererContext* pContext, UINT splitNum, UINT size)
{
	ShadowMap* pShadowMap = new ShadowMap(pContext, splitNum, size);

	if (pShadowMap->Init())
	{
		return pShadowMap;
	}

	delete pShadowMap;
	return nullptr;
}

ShadowMap::ShadowMap(RendererContext* pContext, UINT splitNum, UINT size)
	: m_pContext(pContext)
	, m_pShadowMap(nullptr)
	, m_pShadowMapDSV(nullptr)
	, m_pShadowMapSRV(nullptr)
	, m_pPSShadowMap(nullptr)
	, m_pPSShadowMapSRV(nullptr)
	, m_splitNum(splitNum)
	, m_size(size)
{}

ShadowMap::~ShadowMap()
{
	SafeRelease(m_pPSShadowMapSRV);
	for (auto& pDSV : m_PSShadowMapDSVs)
	{
		SafeRelease(pDSV);
	}
	SafeRelease(m_pPSShadowMap);

	SafeRelease(m_pShadowMapSRV);
	SafeRelease(m_pShadowMapDSV);
	SafeRelease(m_pShadowMap);
}

bool ShadowMap::Init()
{
	ID3D11Device* pDevice = m_pContext->GetDevice();

	D3D11_TEXTURE2D_DESC shadowMapDesc = CreateDefaultTexture2DDesc(
		DXGI_FORMAT_R24G8_TYPELESS,
		m_size, m_size,
		D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE
	);

	HRESULT hr = pDevice->CreateTexture2D(&shadowMapDesc, nullptr, &m_pShadowMap);

	if (SUCCEEDED(hr))
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Flags = 0;

		hr = pDevice->CreateDepthStencilView(m_pShadowMap, &dsvDesc, &m_pShadowMapDSV);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

		hr = pDevice->CreateShaderResourceView(m_pShadowMap, &srvDesc, &m_pShadowMapSRV);
	}


	D3D11_TEXTURE2D_DESC psShadowMapDesc = CreateDefaultTexture2DDesc(
		DXGI_FORMAT_R24G8_TYPELESS,
		m_size, m_size,
		D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE
	);
	psShadowMapDesc.ArraySize = 4u;

	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateTexture2D(&psShadowMapDesc, nullptr, &m_pPSShadowMap);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.ArraySize = 1;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Flags = 0;

		m_PSShadowMapDSVs.resize(4u);

		for (UINT i = 0; i < 4u && SUCCEEDED(hr); ++i)
		{
			dsvDesc.Texture2DArray.FirstArraySlice = i;

			hr = pDevice->CreateDepthStencilView(m_pPSShadowMap, &dsvDesc, &m_PSShadowMapDSVs[i]);
		}
	}

	if (SUCCEEDED(hr))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = 4u;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

		hr = pDevice->CreateShaderResourceView(m_pPSShadowMap, &srvDesc, &m_pPSShadowMapSRV);
	}

	return SUCCEEDED(hr); 
}


ID3D11DepthStencilView* ShadowMap::GetShadowMapSplitDSV(UINT splitIdx) const
{
	return m_pShadowMapDSV;

	if (m_splitNum < splitIdx)
	{
		return nullptr;
	}

	return m_PSShadowMapDSVs[splitIdx];
}


ID3D11ShaderResourceView* ShadowMap::GetShadowMapSRV() const
{
	return m_pShadowMapSRV;
}


void ShadowMap::CalculateVpMatrixForDirectionalLight(const DirectX::XMFLOAT3& direction, DirectX::XMMATRIX& vpMatrix) const
{
	DirectX::XMVECTOR dir = { direction.x, direction.y, direction.z, 0.0f };
	dir = DirectX::XMVector3Normalize(dir);

	DirectX::XMVECTOR x = dir.m128_f32[0] > 0.999f
		? DirectX::XMVECTOR({ 0.0f, 0.0f, 1.0f, 0.0f })
		: DirectX::XMVECTOR({ 1.0f, 0.0f, 0.0f, 0.0f });

	DirectX::XMVECTOR y = DirectX::XMVector3Cross(dir, x);

	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixInverse(nullptr,
		{
			x.m128_f32[0],				x.m128_f32[1],				x.m128_f32[2],				0.0f,
			y.m128_f32[0],				y.m128_f32[1],				y.m128_f32[2],				0.0f,
			dir.m128_f32[0],			dir.m128_f32[1],			dir.m128_f32[2],			0.0f,
			-dir.m128_f32[0] * 10.0f,	-dir.m128_f32[1] * 10.0f,	-dir.m128_f32[3] * 10.0f,	1.0f
		}
	);


	float kWidth = 20.0f;
	float kHeight = 20.0f;
	float kFar = 1000.0f;
	float kNear = 0.1f;

	DirectX::XMMATRIX projMatrix =
	{
		2.0f / kWidth,	0.0f,			0.0f,						0.0f,
		0.0f,			2.0f / kHeight,	0.0f,						0.0f,
		0.0f,			0.0f,			1.0f / (kFar - kNear),		0.0f,
		0.0f,			0.0f,			-kNear / (kFar - kNear),	1.0f
	};

	vpMatrix = viewMatrix * projMatrix;
}


void ShadowMap::Clear()
{
	m_pContext->GetContext()->ClearDepthStencilView(m_pShadowMapDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0u);

	for (auto& pDSV : m_PSShadowMapDSVs)
	{
		m_pContext->GetContext()->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0u);
	}
}
