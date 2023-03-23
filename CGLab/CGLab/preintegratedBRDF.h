#pragma once
#include "framework.h"
#include "rendererContext.h"

class PreintegratedBRDF
{
public:
	static PreintegratedBRDF* Create(RendererContext* pContext);
	HRESULT CalculatePreintegratedBRDF(
		ID3D11Texture2D** ppPBRDFTexture,
		ID3D11ShaderResourceView** ppPBRDFTextureSRV
	);


	~PreintegratedBRDF();
private:
	PreintegratedBRDF(RendererContext* pContext);

	HRESULT CreatePipelineStateObjects();

private:
	const UINT m_preintegratedBRDFsize = 128u;

	RendererContext* m_pContext;

	ID3D11RenderTargetView* m_pPBRDFTextureRTV;

	ID3D11VertexShader* m_pPreintegratedBRDFVS;
	ID3D11PixelShader* m_pPreintegratedBRDFPS;

	ID3D11RasterizerState* m_pRasterizerState;
};
