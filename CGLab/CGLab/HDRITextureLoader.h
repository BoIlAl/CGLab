#pragma once
#include "framework.h"
#include "rendererContext.h"


class HDRITextureLoader
{
public:
	static HDRITextureLoader* CreateHDRITextureLoader(
		RendererContext* pContext,
		UINT cubeTextureSize,
		UINT irradianceMapSize,
		UINT prefilteredTextureSize
	);

	~HDRITextureLoader();

	HRESULT LoadTextureCubeFromHDRI(
		const std::string& fileName,
		ID3D11Texture2D** ppTextureCube,
		ID3D11ShaderResourceView** ppTextureCubeSRV
	);

	HRESULT CalculateIrradianceMap(
		ID3D11ShaderResourceView* pTextureCubeSRV,
		ID3D11Texture2D** ppIrradianceMap,
		ID3D11ShaderResourceView** ppIrradianceMapSRV
	);

	HRESULT CalculatePrefilteredColor(
		ID3D11ShaderResourceView* pEnvironmentSRV,
		ID3D11Texture2D** ppPrefilteredColor,
		ID3D11ShaderResourceView** ppPrefilteredColorSRV
	);

private:
	HDRITextureLoader(
		RendererContext* pContext,
		UINT cubeTextureSize,
		UINT irradianceMapSize,
		UINT prefilteredTextureSize
	);

	bool Init();

	HRESULT CreatePipelineStateObjects();
	HRESULT CreateResources();

	void Render(ID3D11ShaderResourceView* pHDRTextureSrcSRV, ID3D11Texture2D* pTextureCube);
	void RenderIrradiance(ID3D11ShaderResourceView* pEnvironmentTextureSRV, ID3D11Texture2D* pIrradianceCube);
	void RenderPrefiltered(ID3D11ShaderResourceView* pEnvironmentTextureSRV, ID3D11Texture2D* pPrefilteredCube);

private:
	RendererContext* m_pContext;

	ID3D11RasterizerState* m_pRasterizerState;

	ID3D11InputLayout* m_pInputLayout;

	ID3D11VertexShader* m_pToCubeVS;
	ID3D11PixelShader* m_pToCubePS;

	ID3D11VertexShader* m_pIrradianceVS;
	ID3D11PixelShader* m_pIrradiancePS;

	ID3D11VertexShader* m_pPrefilteredVS;
	ID3D11PixelShader* m_pPrefilteredPS;

	ID3D11SamplerState* m_pMinMagLinearSampler;

	ID3D11Texture2D* m_pTmpCubeEdge;
	ID3D11RenderTargetView* m_pTmpCubeEdgeRTV;

	ID3D11Buffer* m_pConstantBuffer;
	ID3D11Buffer* m_pRoughnessBuffer;

	UINT m_cubeTextureSize;
	UINT m_irradianceMapSize;
	UINT m_prefilteredTextureSize;

	UINT m_rougnessValuesNum;

	DirectX::XMMATRIX m_edgesModelMatrices[6];
	DirectX::XMMATRIX m_edgesViewMatrices[6];
	DirectX::XMMATRIX m_projMatrix;
};
