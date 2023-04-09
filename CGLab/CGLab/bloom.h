#pragma once

#include "framework.h"
#include "rendererContext.h"

class Bloom
{
public:
	static Bloom* Create(RendererContext* pContext);

	HRESULT CalculateBloom(
		ID3D11ShaderResourceView* pHDRTextureSRV,
		ID3D11ShaderResourceView* pEmissiveSRV,
		UINT renderTargetWidth,
		UINT renderTargetHeight,
		ID3D11Texture2D** ppBloom,
		ID3D11ShaderResourceView** ppBloomSRV
	);

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

	void RenderBloom(
		ID3D11ShaderResourceView* pHDRTextureSRV,
		ID3D11ShaderResourceView* pBloomMaskSRV,
		ID3D11RenderTargetView* pBloomRTV,
		UINT renderTargetWidth,
		UINT renderTargetHeight
	);


	HRESULT GaussBlurVertical(
		ID3D11ShaderResourceView* pBloomMaskSRV,
		UINT renderTargetWidth,
		UINT renderTargetHeight,
		ID3D11Texture2D** ppBlurResult,
		ID3D11ShaderResourceView** ppBlurResultSRV
		);

	HRESULT GaussBlurHorizontal(
		ID3D11ShaderResourceView* pBloomMaskSRV,
		UINT renderTargetWidth,
		UINT renderTargetHeight,
		ID3D11Texture2D** ppBlurResult,
		ID3D11ShaderResourceView** ppBlurResultSRV
	);

	void RenderBlurVertical(
		ID3D11ShaderResourceView* pBloomMaskSRV,
		ID3D11RenderTargetView* pBlurResultRTV,
		UINT renderTargetWidth,
		UINT renderTargetHeight
	);

	void RenderBlurHorizontal(
		ID3D11ShaderResourceView* pBloomMaskSRV,
		ID3D11RenderTargetView* pBlurResultRTV,
		UINT renderTargetWidth,
		UINT renderTargetHeight
	);

private:
	static constexpr float m_brightnessThreshold = 1.3f;
	static constexpr size_t m_blurSteps = 10;

	RendererContext* m_pContext;

	ID3D11Buffer* m_pBrightnessThresholdBuffer;
	ID3D11Buffer* m_pTextureSizeBuffer;

	ID3D11RasterizerState* m_pRasterizerState;

	ID3D11SamplerState* m_pMinMagMipPointSampler;

	ID3D11PixelShader* m_pBloomMaskPS;
	ID3D11VertexShader* m_pBloomMaskVS;

	ID3D11PixelShader* m_pGaussBlurVerticalPS;
	ID3D11VertexShader* m_pGaussBlurVerticalVS;
	ID3D11PixelShader* m_pGaussBlurHorizontalPS;
	ID3D11VertexShader* m_pGaussBlurHorizontalVS;

	ID3D11PixelShader* m_pBloomPS;
	ID3D11VertexShader* m_pBloomVS;

};