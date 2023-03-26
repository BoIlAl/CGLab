#pragma once
#include "rendererContext.h"


class Environment
{
public:
	enum class Type
	{
		kColorTexture = 0,
		kIrradianceMap,
		kPrefilteredColorTexture
	};

public:
	static Environment* CreateEnvironment(RendererContext* pContext, const std::string& textureFileName);

	~Environment();

	ID3D11Texture2D* GetTexture(Type type) const;
	ID3D11ShaderResourceView* GetTextureSRV(Type type) const;

private:
	Environment();

	bool Init(RendererContext* pContext, const std::string& textureFileName);

private:
	ID3D11Texture2D* m_pEvironmentCube;
	ID3D11ShaderResourceView* m_pEnvironmentCubeSRV;

	ID3D11Texture2D* m_pIrradianceMap;
	ID3D11ShaderResourceView* m_pIrradianceMapSRV;

	ID3D11Texture2D* m_pPrefilteredColor;
	ID3D11ShaderResourceView* m_pPrefilteredColorSRV;
};
