#pragma once
#include "framework.h"
#include "rendererContext.h"


class HDRITextureLoader
{
public:
	static HDRITextureLoader* CreateHDRITextureLoader(
		RendererContext* pContext,
		UINT cubeTextureSize,
		UINT irradianceMapSize
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

private:
	HDRITextureLoader(
		RendererContext* pContext,
		UINT cubeTextureSize,
		UINT irradianceMapSize
	);

	bool Init();

	HRESULT CreatePipelineStateObjects();
	HRESULT CreateResources();

	void RenderIrradiance(ID3D11ShaderResourceView* pHDRTextureSrcSRV, ID3D11Texture2D* pTextureCube, UINT textureSize);
	void Render(ID3D11ShaderResourceView* pHDRTextureSrcSRV, ID3D11Texture2D* pTextureCube, UINT textureSize);

private:
	RendererContext* m_pContext;

	ID3D11RasterizerState* m_pRasterizerState;

	ID3D11InputLayout* m_pInputLayout;

	ID3D11VertexShader* m_pToCubeVS;
	ID3D11PixelShader* m_pToCubePS;

	ID3D11VertexShader* m_pIrradianceVS;
	ID3D11PixelShader* m_pIrradiancePS;

	ID3D11SamplerState* m_pMinMagLinearSamplerState;

	ID3D11Texture2D* m_pTmpCubeEdge;
	ID3D11RenderTargetView* m_pTmpCubeEdgeRTV;

	ID3D11Texture2D* m_pTmpCubeEdge32;
	ID3D11RenderTargetView* m_pTmpCubeEdge32RTV;

	ID3D11Buffer* m_pConstantBuffer;

	UINT m_cubeTextureSize;
	UINT m_irradianceMapSize;

	DirectX::XMMATRIX m_edgesModelMatrices[6];
	DirectX::XMMATRIX m_edgesViewMatrices[6];
	DirectX::XMMATRIX m_projMatrix;
};
