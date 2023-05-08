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

	ID3D11DepthStencilView* GetShadowMapDSVArray() const;
	ID3D11ShaderResourceView* GetShadowMapSRVArray() const;

	inline const std::vector<float>& GetShadowMapSplitDists() const { return m_splitsDists; }

	void CalculateProjMatrixForDirectionalLight(const Box& size, DirectX::XMMATRIX& projMatrix) const;
	void CalculateViewMatrixForDirectionalLight(
		const DirectX::XMFLOAT3& direction,
		float centerX, float centerY, float centerZ,
		DirectX::XMMATRIX& viewMatrix
	) const;

	void CalculatePSSMVpMatricesForDirectionalLight(
		const Camera* pCamera,
		float cameraFov, float cameraAspectRatio,
		float cameraNearPlane, float cameraFarPlane,
		const DirectX::XMFLOAT3& lightDirection,
		DirectX::XMMATRIX* pVpMatrices
	);

	void Clear(RendererContext* pContext);

	inline UINT GetShadowMapSplitsNum() const { return m_splitsNum; }
	inline UINT GetShadowMapTextureSize() const { return m_size; }

	inline float GetLogUniformSplitsInterpolationValue() const { return m_lambda; }
	void SetLogUniformSplitsInterpolationValue(float lambda);

private:
	ShadowMap(UINT splitNum, UINT size);

	bool Init(RendererContext* pContext);
	void CalculateSplitsDists(float nearPlane, float farPlane);

	void BuildProjMatrixForDirectionalLight(
		const DirectX::XMFLOAT3& lightDirection,
		const std::vector<DirectX::XMVECTOR>& cameraBoxPoints,
		float centerX, float centerY, float centerZ,
		DirectX::XMMATRIX& projMatrix
	) const;

private:
	ID3D11Texture2D* m_pPSShadowMap;
	ID3D11DepthStencilView* m_pPSShadowMapDSV;
	ID3D11ShaderResourceView* m_pPSShadowMapSRV;

	UINT m_splitsNum;
	std::vector<float> m_splitsDists;
	UINT m_size;
	float m_lambda;
};

