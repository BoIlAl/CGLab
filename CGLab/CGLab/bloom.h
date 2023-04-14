#pragma once

#include "framework.h"
#include "rendererContext.h"

class Bloom
{
public:
	static Bloom* Create(RendererContext* pContext, UINT targetWidth, UINT targetHeight);

	HRESULT Resize(UINT newTargetWidth, UINT newTargetHeight);

	void CalculateBloom(
		ID3D11ShaderResourceView* pHDRTextureSRV,
		ID3D11ShaderResourceView* pEmissiveSRV,
		ID3D11RenderTargetView* pTargetTextureRTV
	);

	~Bloom();

private:
	Bloom(RendererContext* pContext, UINT targetWidth, UINT targetHeight);

	HRESULT CreatePipelineStateObjects();
	HRESULT CreateResources();

	HRESULT UpdatePingPongTextures();

	void ReleasePingPongResources();

	void RenderBloomMask(ID3D11ShaderResourceView* pHDRTextureSRV, ID3D11ShaderResourceView* pEmissiveSRV);
	void Blur();
	void AddBloom(ID3D11RenderTargetView* pTargetRTV);

private:
	static constexpr float m_brightnessThreshold = 2.0f;
	static constexpr size_t m_blurSteps = 10;

	RendererContext* m_pContext;

	UINT m_width;
	UINT m_height;
	UINT m_blurTextureWidth;
	UINT m_blurTextureHeight;

	ID3D11RasterizerState* m_pRasterizerState;
	ID3D11BlendState* m_pBlendState;

	ID3D11PixelShader* m_pBloomMaskPS;
	ID3D11VertexShader* m_pBloomMaskVS;

	ID3D11PixelShader* m_pGaussBlurVerticalPS;
	ID3D11VertexShader* m_pGaussBlurVerticalVS;
	ID3D11PixelShader* m_pGaussBlurHorizontalPS;
	ID3D11VertexShader* m_pGaussBlurHorizontalVS;

	ID3D11PixelShader* m_pBloomPS;
	ID3D11VertexShader* m_pBloomVS;

	ID3D11Buffer* m_pBloomConstantBuffer;

	ID3D11SamplerState* m_pMinMagMipPointSampler;
	ID3D11SamplerState* m_pMinMagLinearSampler;

	ID3D11Texture2D* m_pingPongBlurTextures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_pingPongBlurTexturesSRV[2] = { nullptr, nullptr };
	ID3D11RenderTargetView* m_pingPongBlurTexturesRTV[2] = { nullptr, nullptr };
};