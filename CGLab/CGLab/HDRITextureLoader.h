#pragma once
#include "framework.h"
#include "rendererContext.h"


class HDRITextureLoader
{
public:
	static HDRITextureLoader* CreateHDRITextureLoader(RendererContext* pContext);

	~HDRITextureLoader();

	HRESULT LoadTextureCubeFromHDRI(
		const std::string& fileName,
		ID3D11Texture2D** ppTextureCube,
		UINT cubeTextureSize,
		ID3D11ShaderResourceView** ppTextureCubeSRV
	);

private:
	HDRITextureLoader(RendererContext* pContext);

	bool Init();

	HRESULT CreatePipelineStateObjects();
	HRESULT CreateResources();

	void Render(ID3D11ShaderResourceView* pHDRTextureSrcSRV, ID3D11Texture2D* pTextureCube, UINT textureSize);

private:
	RendererContext* m_pContext;

	ID3D11RasterizerState* m_pRasterizerState;

	ID3D11InputLayout* m_pInputLayout;

	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader* m_pPixelShader;

	ID3D11SamplerState* m_pMinMagLinearSamplerState;

	ID3D11Texture2D* m_pTmpCubeEdge;
	ID3D11RenderTargetView* m_pTmpCubeEdgeRTV;

	ID3D11Buffer* m_pConstantBuffer;

	DirectX::XMMATRIX m_edgesModelMatrices[6];
	DirectX::XMMATRIX m_edgesViewMatrices[6];
	DirectX::XMMATRIX m_projMatrix;
};
