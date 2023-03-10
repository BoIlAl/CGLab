#pragma once

#include "framework.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11Buffer;
struct ID3DUserDefinedAnnotation;

class ShaderCompiler;


class ToneMapping
{
public:
	static ToneMapping* CreateToneMapping(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, 
									      ShaderCompiler* pShaderCompiler, UINT maxWindowSize);

	~ToneMapping();

	HRESULT ToneMap(
		ID3D11ShaderResourceView* pSrcTextureSRV,
		ID3D11RenderTargetView* pDstTextureRTV,
		UINT renderTargetWidth,
		UINT renderTargetHeight,
		FLOAT deltaTime
	);

	void UseAnnotations(ID3DUserDefinedAnnotation* pAnnotation);

private:
	ToneMapping(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, UINT maxWindowSize);

	HRESULT CreatePipelineStateObjects(ShaderCompiler* pShaderCompiler);
	HRESULT CreateResources();

	FLOAT EyeAdaptation(FLOAT currentExposure, FLOAT deltaTime) const;

	void Update(FLOAT currentExposure, FLOAT deltaTime);

	float CalculateAverageBrightness(
		ID3D11ShaderResourceView* pSrcTextureSRV,
		UINT renderTargetWidth,
		UINT renderTargetHeight);

	UINT DefaineMostDetailedMip(UINT width, UINT height) const;

	void BeginEvent(LPCWSTR name) const;
	void EndEvent() const;

private:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

	ID3D11RasterizerState* m_pRasterizerState;
	ID3D11SamplerState* m_pMinMagLinearSampler;

	ID3D11VertexShader* m_pToneMappingVS;
	ID3D11PixelShader* m_pToneMappingPS;

	ID3D11Texture2D* m_pExposureTexture;
	ID3D11Texture2D* m_pExposureDstTexture;
	std::vector<ID3D11ShaderResourceView*> m_exposureTextureSRVs;
	std::vector<ID3D11RenderTargetView*> m_exposureTextureRTVs;

	ID3D11VertexShader* m_pAverageBrightnessVS;
	ID3D11PixelShader* m_pAverageBrightnessPS;

	ID3D11VertexShader* m_pDownSampleVS;
	ID3D11PixelShader* m_pDownSamplePS;

	ID3DUserDefinedAnnotation* m_pAnnotation;

	UINT m_textureSize;
	UINT m_mipsNum;
	UINT m_mostDetailedMip;

	ID3D11Buffer* m_pExposureBuffer;
	FLOAT m_adaptedBrightness;
};
