#pragma once

#include "framework.h"
#include "light.h"

struct Vertex;
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


static constexpr UINT MaxLightNum = 3;


class Renderer
{
public:
	static Renderer* CreateRenderer(HWND hWnd);
	static void DeleterRenderer(Renderer*& pRenderer);

	void Release();

	~Renderer();

	void Render();
	bool Resize(UINT newWidth, UINT newHeight);

	void ChangeLightBrightness(UINT lightIdx, FLOAT newBrightness);

	Camera* getCamera();

private:
	Renderer();

	bool Init(HWND hWnd);

	HRESULT CreateDevice(IDXGIFactory* pFactory);
	HRESULT CreateSwapChain(IDXGIFactory* pFactory, HWND hWnd);
	HRESULT CreateBackBuffer();
	HRESULT CreatePipelineStateObjects();

	HRESULT CreateCubeResourses();
	HRESULT CreatePlaneResourses();

	HRESULT CreateSceneResources();

	HRESULT SetResourceName(ID3D11Resource* pResource, const std::string& name);

	void Update();
	void RenderScene();
	void PostProcessing();

	void FillLightBuffer();

private:
	static constexpr UINT s_swapChainBuffersNum = 2u;
	static constexpr FLOAT s_PI = 3.14159265359f;

	static constexpr FLOAT s_near = 0.001f;
	static constexpr FLOAT s_far = 1000.0f;
	static constexpr FLOAT s_fov = s_PI / 2.0f;

private:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

	IDXGISwapChain* m_pSwapChain;
	ID3D11RenderTargetView* m_pBackBufferRTV;
	ID3D11Texture2D* m_pDepthTexture;
	ID3D11DepthStencilView* m_pDepthTextureDSV;

	ID3D11Texture2D* m_pHDRRenderTarget;
	ID3D11RenderTargetView* m_pHDRTextureRTV;
	ID3D11ShaderResourceView* m_pHDRTextureSRV;

	ID3D11RasterizerState* m_pRasterizerState;
	ID3D11DepthStencilState* m_pDepthStencilState;

	ID3D11Buffer* m_pCubeVertexBuffer;
	ID3D11Buffer* m_pCubeIndexBuffer;
	UINT m_cubeIndexCount;

	ID3D11Buffer* m_pPlaneVertexBuffer;
	ID3D11Buffer* m_pPlaneIndexBuffer;
	UINT m_planeIndexCount;

	ID3D11Buffer* m_pCubeConstantBuffer;
	ID3D11Buffer* m_pPlaneConstantBuffer;
	ID3D11Buffer* m_pLightBuffer;

	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader* m_pPixelShader;

	ID3D11InputLayout* m_pInputLayout;

	UINT m_windowWidth;
	UINT m_windowHeight;

	ShaderCompiler* m_pShaderCompiler;

	ID3DUserDefinedAnnotation* m_pAnnotation;

	size_t m_startTime;
	size_t m_currentTime;
	size_t m_timeFromLastFrame;

	Camera* m_pCamera;

	bool m_isDebug;

	ToneMapping* m_pToneMapping;

	std::vector<PointLight> m_lights;
};

