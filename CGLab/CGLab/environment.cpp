#include "environment.h"

#include "framework.h"


Environment* Environment::CreateEnvironment(RendererContext* pContext, const std::string& textureFileName)
{
	Environment* pEnvironment = new Environment();

	if (pEnvironment->Init(pContext, textureFileName))
	{
		return pEnvironment;
	}

	delete pEnvironment;
	return nullptr;
}


Environment::Environment()
	: m_pEvironmentCube(nullptr)
	, m_pEnvironmentCubeSRV(nullptr)
	, m_pIrradianceMap(nullptr)
	, m_pIrradianceMapSRV(nullptr)
	, m_pPrefilteredColor(nullptr)
	, m_pPrefilteredColorSRV(nullptr)
{}

Environment::~Environment()
{
	SafeRelease(m_pPrefilteredColorSRV);
	SafeRelease(m_pPrefilteredColor);
	SafeRelease(m_pIrradianceMapSRV);
	SafeRelease(m_pIrradianceMap);
	SafeRelease(m_pEnvironmentCubeSRV);
	SafeRelease(m_pEvironmentCube);
}


bool Environment::Init(RendererContext* pContext, const std::string& textureFileName)
{
	HRESULT hr = pContext->LoadTextureCubeFromHDRI(
		textureFileName,
		&m_pEvironmentCube,
		&m_pEnvironmentCubeSRV
	);

	if (SUCCEEDED(hr))
	{
		hr = pContext->CalculateIrradianceMap(
			m_pEnvironmentCubeSRV,
			&m_pIrradianceMap,
			&m_pIrradianceMapSRV
		);
	}

	if (SUCCEEDED(hr))
	{
		hr = pContext->CalculatePrefilteredColor(
			m_pEnvironmentCubeSRV,
			&m_pPrefilteredColor,
			&m_pPrefilteredColorSRV
		);
	}

	return SUCCEEDED(hr);
}


ID3D11Texture2D* Environment::GetTexture(Environment::Type type) const
{
	switch (type)
	{
	case Environment::Type::kColorTexture:
		return m_pEvironmentCube;

	case Environment::Type::kIrradianceMap:
		return m_pIrradianceMap;

	case Environment::Type::kPrefilteredColorTexture:
		return m_pPrefilteredColor;
	}

	return nullptr;
}

ID3D11ShaderResourceView* Environment::GetTextureSRV(Environment::Type type) const
{
	switch (type)
	{
	case Environment::Type::kColorTexture:
		return m_pEnvironmentCubeSRV;

	case Environment::Type::kIrradianceMap:
		return m_pIrradianceMapSRV;

	case Environment::Type::kPrefilteredColorTexture:
		return m_pPrefilteredColorSRV;
	}

	return nullptr;
}
