#pragma once

#include "framework.h"
#include "rendererContext.h"

class Bloom
{
public:
	static Bloom* Create(RendererContext* pContext);

	~Bloom();

private:
	Bloom(RendererContext* pContext);

	HRESULT CreatePipelineStateObjects();
	HRESULT CreateResources();

	HRESULT CalculateBloomMask(
		ID3D11ShaderResourceView* pHDRTextureSRV,
		ID3D11ShaderResourceView* pEmissiveSRV,
		UINT renderTargetWidth,
		UINT renderTargetHeight,
		ID3D11Texture2D** ppBloomMask,
		ID3D11ShaderResourceView** ppBloomMaskSRV
	);

	void RenderBloomMask(
		ID3D11ShaderResourceView* pHDRTextureSRV,
		ID3D11ShaderResourceView* pEmissiveSRV,
		ID3D11RenderTargetView* pBloomMaskRTV,
		UINT renderTargetWidth,
		UINT renderTargetHeight
	);

private:
	static constexpr float m_pBrightnessThreshold = 1.3f;

	RendererContext* m_pContext;

	ID3D11Buffer* m_pBrightnessThresholdBuffer;
	ID3D11RasterizerState* m_pRasterizerState;

	ID3D11PixelShader* m_pBloomMaskPS;
	ID3D11VertexShader* m_pBloomMaskVS;
};