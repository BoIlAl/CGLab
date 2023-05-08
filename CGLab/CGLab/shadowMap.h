#pragma once
#include "framework.h"

class RendererContext;
class Camera;


class ShadowMap
{
public:
	struct Box
	{
		Box() = default;
		Box(const std::vector<DirectX::XMVECTOR>& points);

		float right = 0.0f, left = 0.0f;
		float top = 0.0f, bottom = 0.0f;
		float nearPlane = 0.0f, farPlane = 0.0f;
	};

public:
	static ShadowMap* CreateShadowMap(RendererContext* pContext, UINT splitNum, UINT size = 2048u);

	~ShadowMap();

	ID3D11DepthStencilView* GetShadowMapSplitDSV(UINT splitIdx) const;
	ID3D11DepthStencilView* GetShadowMapDSVArray() const;
	ID3D11ShaderResourceView* GetShadowMapSRV() const;

	inline const std::vector<float>& GetShadowMapSplitDists() const { return m_splitDists; }

	void CalculateProjMatrixForDirectionalLight(const Box& size, DirectX::XMMATRIX& projMatrix) const;
	void CalculateViewMatrixForDirectionalLight(
		const DirectX::XMFLOAT3& direction,
		float centerX, float centerY, float centerZ,
		DirectX::XMMATRIX& viewMatrix
	) const;
	void CalculateVpMatrixForDirectionalLight(const DirectX::XMFLOAT3& direction, DirectX::XMMATRIX& vpMatrix) const;

	DirectX::XMMATRIX CalculatePSSMForDirectioanlLight(
		const Camera* pCamera,
		float fov, float aspectRation,
		float nearPlane, float farPlane,
		const DirectX::XMFLOAT3& direction
	) const;

	void CalculatePSSMVpMatricesForDirectionalLight(
		const Camera* pCamera,
		float cameraFov, float cameraAspectRatio,
		float cameraNearPlane, float cameraFarPlane,
		const DirectX::XMFLOAT3& lightDirection,
		DirectX::XMMATRIX* pVpMatrices
	) const;

	void Clear();

	inline UINT GetShadowMapSplitsNum() const { return m_splitsNum; }
	inline UINT GetShadowMapTextureSize() const { return m_size; }

private:
	ShadowMap(RendererContext* pContext, UINT splitNum, UINT size);

	bool Init();
	void CalculateSplits();

	void BuildProjMatrixForDirectionalLight(
		const DirectX::XMFLOAT3& lightDirection,
		const std::vector<DirectX::XMVECTOR>& cameraBoxPoints,
		float centerX, float centerY, float centerZ,
		DirectX::XMMATRIX& projMatrix
	) const;

private:
	static constexpr float s_near = 0.1f;
	static constexpr float s_far = 100.0f;

private:
	RendererContext* m_pContext;

	ID3D11Texture2D* m_pShadowMap;
	ID3D11DepthStencilView* m_pShadowMapDSV;
	ID3D11ShaderResourceView* m_pShadowMapSRV;

	ID3D11Texture2D* m_pPSShadowMap;
	std::vector<ID3D11DepthStencilView*> m_PSShadowMapDSVs;
	ID3D11DepthStencilView* m_pPSShadowMapDSV;
	ID3D11ShaderResourceView* m_pPSShadowMapSRV;

	UINT m_splitsNum;
	std::vector<float> m_splitDists;
	UINT m_size;


};

