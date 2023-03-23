#pragma once
#include "framework.h"
#include "rendererContext.h"

class PrefilteredColor
{
public:
	static PrefilteredColor* Create(RendererContext* pContext);
	HRESULT CalculatePrefilteredColor(
		ID3D11Texture2D* pEnvironment,
		ID3D11ShaderResourceView* pEnvironmentSRV,
		ID3D11Texture2D** ppPrefilteredColor,
		ID3D11ShaderResourceView** ppPrefilteredColorSRV
	);


	~PrefilteredColor();
private:
	PrefilteredColor(RendererContext* pContext);

	HRESULT CreatePipelineStateObjects();
	HRESULT CreateResources();
private:
	const std::vector<FLOAT> m_roughnessValues = {0.0, 0.25, 0.5, 0.75, 1.0};
	const UINT m_prefilteredColorSize = 128u;

	RendererContext* m_pContext;

	ID3D11VertexShader* m_pPrefilteredColorVS;
	ID3D11PixelShader* m_pPrefilteredColorPS;
	
	ID3D11RasterizerState* m_pRasterizerState;

	ID3D11SamplerState* m_pMinMagLinearSampler;

	ID3D11Buffer* m_pRoughnessBuffer;
	ID3D11Buffer* m_pConstantBuffer;

	ID3D11Texture2D* m_pTextureCubeResource;
	ID3D11ShaderResourceView* m_pTextureCubeRecourceSRV;

	ID3D11Texture2D* m_pTextureCubeTarget;
	std::vector <ID3D11RenderTargetView*> m_pTextureCubeTargets;

	DirectX::XMMATRIX m_edgesModelMatrices[6];
	DirectX::XMMATRIX m_edgesViewMatrices[6];
	DirectX::XMMATRIX m_projMatrix;
};


