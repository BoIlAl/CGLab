#pragma once

#include "framework.h"
#include "light.h"
#include "common.h"
#include "environment.h"
#include "rendererContext.h"

struct IDXGIFactory;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11Texture2D;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3DUserDefinedAnnotation;
struct ID3D11Resource;

class ShaderCompiler;
class ToneMapping;
class Camera;
class Bloom;
class ShadowMap;

static constexpr UINT MaxLightNum = 3;


class Renderer
{
public:
	static Renderer* CreateRenderer(HWND hWnd);
	static void DeleteRenderer(Renderer*& pRenderer);

	void Release();

	void Render();
	bool Resize(UINT newWidth, UINT newHeight);

	void ChangeLightBrightness(UINT lightIdx, FLOAT newBrightness);

	Camera* GetCamera();
	inline RendererContext* GetContext() const { return m_pContext; }

private:
	Renderer();
	~Renderer();

	bool Init(HWND hWnd);
	bool InitImGui(HWND hWnd);

	void RenderImGui();

	HRESULT CreateSwapChain(IDXGIFactory* pFactory, HWND hWnd);
	HRESULT CreateBackBuffer();
	HRESULT CreatePipelineStateObjects();

	HRESULT CreateCubeResourses(Mesh*& cubeMesh);
	HRESULT CreatePlaneResourses(Mesh*& planeMesh);

	HRESULT CreateSceneResources();

	HRESULT LoadModels();

	HRESULT SetResourceName(ID3D11Resource* pResource, const std::string& name);

	void Update();
	void RenderScene();
	void RenderEnvironment();
	void RenderShadowMap();
	void PostProcessing();

	void FillLightBuffer();

private:
	static constexpr UINT s_swapChainBuffersNum = 2u;

	static constexpr FLOAT s_near = 0.001f;
	static constexpr FLOAT s_far = 1000.0f;
	static constexpr FLOAT s_fov = PI / 2.0f;

private:
	RendererContext* m_pContext;

	IDXGISwapChain* m_pSwapChain;
	ID3D11RenderTargetView* m_pBackBufferRTV;
	ID3D11Texture2D* m_pDepthTexture;
	ID3D11DepthStencilView* m_pDepthTextureDSV;

	ID3D11Texture2D* m_pHDRRenderTarget;
	ID3D11RenderTargetView* m_pHDRTextureRTV;
	ID3D11ShaderResourceView* m_pHDRTextureSRV;

	ID3D11Texture2D* m_pEmissiveTexture;
	ID3D11RenderTargetView* m_pEmissiveTextureRTV;
	ID3D11ShaderResourceView* m_pEmissiveTextureSRV;

	ID3D11RasterizerState* m_pRasterizerState;
	ID3D11RasterizerState* m_pRasterizerStateFront;
	ID3D11DepthStencilState* m_pDepthStencilState;
	ID3D11SamplerState* m_pMinMagMipLinearSampler;
	ID3D11SamplerState* m_pMinMagMipLinearSamplerClamp;
	ID3D11SamplerState* m_pMinMagMipNearestSampler;
	ID3D11SamplerState* m_pShadowMapSampler;

	std::vector<Mesh*> m_meshes;
	Mesh* m_pEnvironmentSphere;

	ID3D11Buffer* m_pConstantBuffer;
	ID3D11Buffer* m_pPBRBuffer;
	ID3D11Buffer* m_pLightBuffer;
	ID3D11Buffer* m_pPSSMConstantBuffer;
	ID3D11Buffer* m_pDebugParamsBuffer;

	ID3D11VertexShader* m_pSceneVShader;
	ID3D11PixelShader* m_pScenePShader;

	ID3D11VertexShader* m_pSceneColorTextureVShader;
	ID3D11PixelShader* m_pSceneColorTexturePShader;

	ID3D11VertexShader* m_pSceneColorEmissiveVShader;
	ID3D11PixelShader* m_pSceneColorEmissivePShader;

	ID3D11VertexShader* m_pEnvironmentVShader;
	ID3D11PixelShader* m_pEnvironmentPShader;

	ID3D11VertexShader* m_pShadowMapVShader;
	ID3D11GeometryShader* m_pShadowMapGShader;

	ID3D11InputLayout* m_pInputLayout;

	ID3D11Texture2D* m_pPBRDFTexture;
	ID3D11ShaderResourceView* m_pPBRDFTextureSRV;

	Environment* m_pEnvironment;


	UINT m_windowWidth;
	UINT m_windowHeight;

	DirectX::XMMATRIX m_projMatrix;

	size_t m_startTime;
	size_t m_currentTime;
	size_t m_timeFromLastFrame;

	Camera* m_pCamera;

	ToneMapping* m_pToneMapping;

	Bloom* m_pBloom;

	ShadowMap* m_pDirectionalLightShadowMap;
	bool m_showPSSMSplits;
	float m_cameraFarPlaneForPSSM;

	std::vector<PointLight> m_lights;
	DirectionalLight m_directionalLight;

	std::vector<Model*> m_models;
};
