#pragma once
#include "framework.h"
#include "rendererContext.h"


class HDRITextureLoader
{
public:
	static HDRITextureLoader* CreateHDRITextureLoader(RendererContext* pContext);

	~HDRITextureLoader();

private:
	HDRITextureLoader(RendererContext* pContext);

	bool Init();

	HRESULT CreatePipelineStateObjects();
	HRESULT CreateResources();

private:
	RendererContext* m_pContext;

	ID3D11RasterizerState* m_pRasterizerState;

	ID3D11InputLayout* m_pInputLayout;

	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader* m_pPixelShader;

	ID3D11Texture2D* m_pTmpCubeEdge;
	ID3D11RenderTargetView* m_pTmpCubeEdgeRTV;

	Mesh* m_pSquareMesh;
};
