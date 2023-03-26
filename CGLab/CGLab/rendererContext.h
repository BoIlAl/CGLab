#pragma once
#include "framework.h"
#include "shaderCompiler.h"
#include "common.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3DUserDefinedAnnotation;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

class PreintegratedBRDFBuilder;
class HDRITextureLoader;


struct Vertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;
	DirectX::XMFLOAT3 normal;
};

struct Mesh
{
	ID3D11Buffer* pVertexBuffer = nullptr;
	ID3D11Buffer* pIndexBuffer = nullptr;
	UINT indexCount = 0;
	DirectX::XMMATRIX modelMatrix;

	~Mesh()
	{
		SafeRelease(pIndexBuffer);
		SafeRelease(pVertexBuffer);
	}
};


class RendererContext
{
public:
	static RendererContext* CreateContext(IDXGIFactory* pFactory);

	~RendererContext();

	inline ID3D11Device* GetDevice() const { return m_pDevice; }
	inline ID3D11DeviceContext* GetContext() const { return m_pContext; }
	inline ShaderCompiler* GetShaderCompiler() const { return m_pShaderCompiler; }

	void BeginEvent(LPCWSTR eventName) const;
	void EndEvent() const;

	HRESULT LoadTextureCube(
		const std::string& pathToCubeSrc,
		ID3D11Texture2D** ppTextureCube,
		ID3D11ShaderResourceView** ppTextureCubeSRV
	) const;

	HRESULT LoadTextureCubeFromHDRI(
		const std::string& fileName,
		ID3D11Texture2D** ppTextureCube,
		ID3D11ShaderResourceView** ppTextureCubeSRV
	) const;

	HRESULT CalculateIrradianceMap(
		ID3D11ShaderResourceView* pEnvironmentCubeSRV,
		ID3D11Texture2D** ppIrradianceMap,
		ID3D11ShaderResourceView** ppIrradianceMapSRV
	) const;

	HRESULT CalculatePrefilteredColor(
		ID3D11ShaderResourceView* pEnvironmentCubeSRV,
		ID3D11Texture2D** ppPrefilteredColor,
		ID3D11ShaderResourceView** ppPrefilteredColorSRV
	) const;

	HRESULT CalculatePreintegratedBRDF(
		ID3D11Texture2D** ppPBRDFTexture,
		ID3D11ShaderResourceView** ppPBRDFTextureSRV
	) const;

	HRESULT CreateSphereMesh(UINT16 latitudeBands, UINT16 longitudeBands, Mesh*& sphereMesh) const;

private:
	RendererContext();

	bool Init(IDXGIFactory* pFactory);

private:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

	ID3DUserDefinedAnnotation* m_pAnnotation;

	ShaderCompiler* m_pShaderCompiler;
	HDRITextureLoader* m_pHDRITextureLoader;

	PreintegratedBRDFBuilder* m_pPreintegratedBRDFBuilder;

	bool m_isDebug;
};
