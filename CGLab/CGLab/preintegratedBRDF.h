#pragma once
#include "framework.h"
#include "rendererContext.h"

class PreintegratedBRDFBuilder
{
public:
	static PreintegratedBRDFBuilder* Create(RendererContext* pContext, UINT maxTextureSize);

	~PreintegratedBRDFBuilder();

	HRESULT CalculatePreintegratedBRDF(
		ID3D11Texture2D** ppPBRDFTexture,
		ID3D11ShaderResourceView** ppPBRDFTextureSRV,
		UINT textureSize
	);


private:
	PreintegratedBRDFBuilder(RendererContext* pContext, UINT maxTextureSize);

	HRESULT CreatePipelineStateObjects();
	HRESULT CreateResources();

	void RenderPreintegratedBRDF(ID3D11Texture2D* pTargetTexture, UINT targetSize);

private:
	RendererContext* m_pContext;

	ID3D11Texture2D* m_pTmpTexture;
	ID3D11RenderTargetView* m_pTmpTextureRTV;

	ID3D11VertexShader* m_pPreintegratedBRDFVS;
	ID3D11PixelShader* m_pPreintegratedBRDFPS;

	ID3D11RasterizerState* m_pRasterizerState;

	UINT m_preintegratedBRDFsize;
};
