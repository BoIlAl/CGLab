#pragma once
#include "framework.h"

class RendererContext;


class ShadowMap
{
public:
	static ShadowMap* CreateShadowMap(RendererContext* pContext, UINT splitNum, UINT size = 2048u);

	~ShadowMap();

	ID3D11DepthStencilView* GetShadowMapSplitDSV(UINT splitIdx) const;
	ID3D11ShaderResourceView* GetShadowMapSRV() const;

	void CalculateVpMatrixForDirectionalLight(const DirectX::XMFLOAT3& direction, DirectX::XMMATRIX& vpMatrix) const;

	void Clear();

	inline UINT GetShadowMapSplitNum() const { return m_splitNum; }
	inline UINT GetShadowMapTextureSize() const { return m_size; }

private:
	ShadowMap(RendererContext* pContext, UINT splitNum, UINT size);

	bool Init();

private:
	RendererContext* m_pContext;

	ID3D11Texture2D* m_pShadowMap;
	ID3D11DepthStencilView* m_pShadowMapDSV;
	ID3D11ShaderResourceView* m_pShadowMapSRV;

	ID3D11Texture2D* m_pPSShadowMap;
	std::vector<ID3D11DepthStencilView*> m_PSShadowMapDSVs;
	ID3D11ShaderResourceView* m_pPSShadowMapSRV;

	UINT m_splitNum;
	UINT m_size;
};

