#include "shadowMap.h"
#include "rendererContext.h"
#include "camera.h"


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
	, m_pPSShadowMapDSV(nullptr)
	, m_pPSShadowMapSRV(nullptr)
	, m_splitsNum(splitNum)
	, m_size(size)
{
	CalculateSplits();
}

ShadowMap::~ShadowMap()
{
	SafeRelease(m_pPSShadowMapSRV);
	SafeRelease(m_pPSShadowMapDSV);
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
	psShadowMapDesc.ArraySize = m_splitsNum;

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

		m_PSShadowMapDSVs.resize(m_splitsNum);

		for (UINT i = 0; i < m_splitsNum && SUCCEEDED(hr); ++i)
		{
			dsvDesc.Texture2DArray.FirstArraySlice = i;

			hr = pDevice->CreateDepthStencilView(m_pPSShadowMap, &dsvDesc, &m_PSShadowMapDSVs[i]);
		}
	}

	if (SUCCEEDED(hr))
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.ArraySize = m_splitsNum;
		dsvDesc.Texture2DArray.FirstArraySlice = 0;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Flags = 0;

		hr = pDevice->CreateDepthStencilView(m_pPSShadowMap, &dsvDesc, &m_pPSShadowMapDSV);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = m_splitsNum;
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

	if (m_splitsNum < splitIdx)
	{
		return nullptr;
	}

	return m_PSShadowMapDSVs[splitIdx];
}


ID3D11DepthStencilView* ShadowMap::GetShadowMapDSVArray() const
{
	return m_pPSShadowMapDSV;
}


ID3D11ShaderResourceView* ShadowMap::GetShadowMapSRV() const
{
	return m_pShadowMapSRV;
}


void ShadowMap::CalculateVpMatrixForDirectionalLight(const DirectX::XMFLOAT3& direction, DirectX::XMMATRIX& vpMatrix) const
{
	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
	//CalculateViewMatrixForDirectionalLight(direction, viewMatrix);

	Box box = {};
	box.top = 10.0f;
	box.bottom = -10.0f;
	box.right = 10.0f;
	box.left = -10.0f;
	box.nearPlane = 0.1f;
	box.farPlane = 1000.0f;

	DirectX::XMMATRIX projMatrix;
	CalculateProjMatrixForDirectionalLight(box, projMatrix);

	vpMatrix = viewMatrix * projMatrix;
}


void ShadowMap::CalculateProjMatrixForDirectionalLight(const Box& size, DirectX::XMMATRIX& projMatrix) const
{
	float width = size.right - size.left;
	float height = size.top - size.bottom;
	float farSubNear = size.farPlane - size.nearPlane;

	projMatrix =
	{
		2.0f / width,						0.0f,									0.0f,							0.0f,
		0.0f,								2.0f / height,							0.0f,							0.0f,
		0.0f,								0.0f,									1.0f / farSubNear,				0.0f,
		-(size.left + size.right) / width,	-(size.top + size.bottom) / height,		-size.nearPlane / farSubNear,	1.0f
	};
}


void ShadowMap::CalculateViewMatrixForDirectionalLight(
	const DirectX::XMFLOAT3& direction,
	float centerX, float centerY, float centerZ,
	DirectX::XMMATRIX& viewMatrix
) const
{
	DirectX::XMVECTOR dir = { direction.x, direction.y, direction.z, 0.0f };
	dir = DirectX::XMVector3Normalize(dir);

	DirectX::XMVECTOR x = dir.m128_f32[0] > 0.999f
		? DirectX::XMVECTOR({ 0.0f, 0.0f, 1.0f, 0.0f })
		: DirectX::XMVECTOR({ 1.0f, 0.0f, 0.0f, 0.0f });

	DirectX::XMVECTOR y = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(dir, x));
	x = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(y, dir));

	DirectX::XMVECTOR center = { centerX, centerY, centerZ, 0.0f };

	viewMatrix = DirectX::XMMatrixLookToLH(
		DirectX::XMVectorSubtract(center, DirectX::XMVectorScale(dir, 4.0f)),
		dir,
		x
	);
}


DirectX::XMMATRIX ShadowMap::CalculatePSSMForDirectioanlLight(
	const Camera* pCamera,
	float fov, float aspectRation,
	float nearPlane, float farPlane,
	const DirectX::XMFLOAT3& direction
) const
{
	DirectX::XMFLOAT4 posFloat4 = pCamera->GetPosition();
	DirectX::XMVECTOR pos = { posFloat4.x, posFloat4.y, posFloat4.z, posFloat4.w };

	DirectX::XMVECTOR cameraDir = pCamera->GetDirection();
	DirectX::XMVECTOR cameraUp = pCamera->GetUp();
	DirectX::XMVECTOR cameraRight = pCamera->GetRight();

	UINT pointIdx = 0;
	std::vector<DirectX::XMVECTOR> points;
	points.resize(8);

	float borderZ[2] = { nearPlane, farPlane };
	std::pair<float, float> borderXYSigns[4] = { { -1.0f, 1.0f }, { 1.0f, 1.0f }, { -1.0f, -1.0f }, { 1.0f, -1.0f } };

	for (UINT i = 0; i < _countof(borderZ); ++i)
	{
		float width = borderZ[i] * tanf(fov * 0.5f);
		float height = width * aspectRation;
		DirectX::XMVECTOR dirScaleBorderZ = DirectX::XMVectorScale(cameraDir, borderZ[i]);

		for (UINT j = 0; j < _countof(borderXYSigns); ++j)
		{
			const auto& signs = borderXYSigns[j];

			points[pointIdx++] = DirectX::XMVectorAdd(
				DirectX::XMVectorAdd(
					DirectX::XMVectorAdd(pos, dirScaleBorderZ), DirectX::XMVectorScale(cameraRight, signs.first * width)
				),
				DirectX::XMVectorScale(cameraUp, signs.second * height)
			);
		}
	}

	Box box1 = Box(points);

	float centerX = 0.5f * (box1.right + box1.left);
	float centerY = 0.5f * (box1.top + box1.bottom);
	float centerZ = 0.5f * (box1.farPlane + box1.nearPlane);

	//DirectX::XMVECTOR ligthPos = { centerX + -direction.x * 4.0f, centerY + -direction.y * 4.0f, centerZ + -direction.z * 4.0f, 0.0f };
	//DirectX::XMVECTOR lightDir = { direction.x, direction.y, direction.z, 0.0f };
	//lightDir = DirectX::XMVector3Normalize(lightDir);

	//DirectX::XMVECTOR lightUp = lightDir.m128_f32[0] > 0.999f
	//	? DirectX::XMVECTOR({ 0.0f, 0.0f, 1.0f, 0.0f })
	//	: DirectX::XMVECTOR({ 1.0f, 0.0f, 0.0f, 0.0f });

	//DirectX::XMVECTOR lightRight = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(lightDir, lightUp));
	//lightUp = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(lightRight, lightDir));

	//Box box = {};
	//box.left = box.bottom = box.nearPlane = FLT_MAX;
	//box.right = box.top = box.farPlane = -FLT_MAX;

	//for (UINT i = 0; i < points.size(); ++i)
	//{
	//	const DirectX::XMVECTOR& sub = DirectX::XMVectorSubtract(points[i], ligthPos);

	//	float x = DirectX::XMVector3Dot(sub, lightRight).m128_f32[0];
	//	float y = DirectX::XMVector3Dot(sub, lightUp).m128_f32[0];
	//	float z = DirectX::XMVector3Dot(sub, lightDir).m128_f32[0];

	//	box.left = min(box.left, x);
	//	box.bottom = min(box.bottom, y);
	//	box.nearPlane = min(box.nearPlane, z);

	//	box.right = max(box.right, x);
	//	box.top = max(box.top, y);
	//	box.farPlane = max(box.farPlane, z);
	//}

	DirectX::XMMATRIX projMatrix;
	//CalculateProjMatrixForDirectionalLight(box, projMatrix);
	BuildProjMatrixForDirectionalLight(direction, points, centerX, centerY, centerZ, projMatrix);

	DirectX::XMMATRIX viewMatrix;
	CalculateViewMatrixForDirectionalLight(direction, centerX, centerY, centerZ, viewMatrix);

	return viewMatrix * projMatrix;
}


void ShadowMap::CalculatePSSMVpMatricesForDirectionalLight(
	const Camera* pCamera,
	float cameraFov, float cameraAspectRatio,
	float cameraNearPlane, float cameraFarPlane,
	const DirectX::XMFLOAT3& lightDirection,
	DirectX::XMMATRIX* pVpMatrices
) const
{
	DirectX::XMFLOAT4 posFloat4 = pCamera->GetPosition();
	DirectX::XMVECTOR pos = { posFloat4.x, posFloat4.y, posFloat4.z, posFloat4.w };

	DirectX::XMVECTOR cameraDir = pCamera->GetDirection();
	DirectX::XMVECTOR cameraUp = pCamera->GetUp();
	DirectX::XMVECTOR cameraRight = pCamera->GetRight();

	auto buildCameraBox = [&](float nearPlane, float farPlane, std::vector<DirectX::XMVECTOR>& boxPoints)
	{
		UINT pointIdx = 0;
		boxPoints.resize(8);

		float borderZ[2] = { nearPlane, farPlane };
		std::pair<float, float> borderXYSigns[4] = { { -1.0f, 1.0f }, { 1.0f, 1.0f }, { -1.0f, -1.0f }, { 1.0f, -1.0f } };

		for (UINT i = 0; i < _countof(borderZ); ++i)
		{
			float width = borderZ[i] * tanf(cameraFov * 0.5f);
			float height = width * cameraAspectRatio;
			DirectX::XMVECTOR dirScaleBorderZ = DirectX::XMVectorScale(cameraDir, borderZ[i]);

			for (UINT j = 0; j < _countof(borderXYSigns); ++j)
			{
				const auto& signs = borderXYSigns[j];

				boxPoints[pointIdx++] = DirectX::XMVectorAdd(
					DirectX::XMVectorAdd(
						DirectX::XMVectorAdd(pos, dirScaleBorderZ), DirectX::XMVectorScale(cameraRight, signs.first * width)
					),
					DirectX::XMVectorScale(cameraUp, signs.second * height)
				);
			}
		}
	};

	for (UINT splitIdx = 0; splitIdx < m_splitsNum; ++splitIdx)
	{
		float nearPlane = s_near;
		float farPlane = m_splitDists[splitIdx];

		std::vector<DirectX::XMVECTOR> points;
		buildCameraBox(nearPlane, farPlane, points);

		Box cameraBox = Box(points);
		float centerX = 0.5f * (cameraBox.left + cameraBox.right);
		float centerY = 0.5f * (cameraBox.bottom + cameraBox.top);
		float centerZ = 0.5f * (cameraBox.nearPlane + cameraBox.farPlane);

		DirectX::XMMATRIX viewMatrix;
		CalculateViewMatrixForDirectionalLight(lightDirection, centerX, centerY, centerZ, viewMatrix);

		DirectX::XMMATRIX projMatrix;
		BuildProjMatrixForDirectionalLight(
			lightDirection,
			points,
			centerX, centerY, centerZ,
			projMatrix
		);

		pVpMatrices[splitIdx] = viewMatrix * projMatrix;
	}
}


void ShadowMap::BuildProjMatrixForDirectionalLight(
	const DirectX::XMFLOAT3& lightDirection,
	const std::vector<DirectX::XMVECTOR>& cameraBoxPoints,
	float centerX, float centerY, float centerZ,
	DirectX::XMMATRIX& projMatrix
) const
{
	DirectX::XMVECTOR ligthPos =
	{
		centerX + -lightDirection.x * 4.0f,
		centerY + -lightDirection.y * 4.0f,
		centerZ + -lightDirection.z * 4.0f,
		0.0f
	};
	DirectX::XMVECTOR lightDir = { lightDirection.x, lightDirection.y, lightDirection.z, 0.0f };
	lightDir = DirectX::XMVector3Normalize(lightDir);

	DirectX::XMVECTOR lightUp = lightDir.m128_f32[0] > 0.999f
		? DirectX::XMVECTOR({ 0.0f, 0.0f, 1.0f, 0.0f })
		: DirectX::XMVECTOR({ 1.0f, 0.0f, 0.0f, 0.0f });

	DirectX::XMVECTOR lightRight = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(lightDir, lightUp));
	lightUp = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(lightRight, lightDir));

	std::vector<DirectX::XMVECTOR> lightSpaceBoxPoints;
	lightSpaceBoxPoints.resize(cameraBoxPoints.size());

	for (UINT i = 0; i < cameraBoxPoints.size(); ++i)
	{
		const DirectX::XMVECTOR& sub = DirectX::XMVectorSubtract(cameraBoxPoints[i], ligthPos);

		lightSpaceBoxPoints[i] =
		{
			DirectX::XMVector3Dot(sub, lightRight).m128_f32[0],
			DirectX::XMVector3Dot(sub, lightUp).m128_f32[0],
			DirectX::XMVector3Dot(sub, lightDir).m128_f32[0],
			0.0f
		};
	}

	Box lightSpaceBox = Box(lightSpaceBoxPoints);

	CalculateProjMatrixForDirectionalLight(lightSpaceBox, projMatrix);
}


void ShadowMap::Clear()
{
	m_pContext->GetContext()->ClearDepthStencilView(m_pShadowMapDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0u);

	for (auto& pDSV : m_PSShadowMapDSVs)
	{
		m_pContext->GetContext()->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0u);
	}
}


void ShadowMap::CalculateSplits()
{
	float farDivNear = s_far / s_near;
	float invSplitNum = 1.0f / m_splitsNum;

	for (UINT i = 1; i <= m_splitsNum; ++i)
	{
		float ciLog = s_near * std::pow(farDivNear, i * invSplitNum);
		float ciUni = s_near + (s_far - s_near) * (i * invSplitNum);
		float ci = 0.5f * ciLog + 0.5f * ciUni;

		m_splitDists.push_back(ci);
	}
}


ShadowMap::Box::Box(const std::vector<DirectX::XMVECTOR>& points)
{
	left = bottom = nearPlane = FLT_MAX;
	right = top = farPlane = -FLT_MAX;

	for (UINT i = 0; i < points.size(); ++i)
	{
		float x = points[i].m128_f32[0];
		float y = points[i].m128_f32[1];
		float z = points[i].m128_f32[2];

		left = min(left, x);
		bottom = min(bottom, y);
		nearPlane = min(nearPlane, z);

		right = max(right, x);
		top = max(top, y);
		farPlane = max(farPlane, z);
	}
}
