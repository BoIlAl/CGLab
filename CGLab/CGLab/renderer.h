#pragma once

#include "framework.h"

struct Vertex;
struct IDXGIFactory;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11Texture2D;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;

class ShaderCompiler;
class Camera;

class Renderer
{
public:
	static Renderer* CreateRenderer(HWND hWnd);
	static void DeleterRenderer(Renderer*& pRenderer);

	void Release();

	~Renderer();

	void Render();
	bool Resize(UINT newWidth, UINT newHeight);

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

	void Update();
	void RenderScene();

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

	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader* m_pPixelShader;

	ID3D11InputLayout* m_pInputLayout;

	UINT m_windowWidth;
	UINT m_windowHeight;

	ShaderCompiler* m_pShaderCompiler;

	size_t m_startTime;
	size_t m_currentTime;

	Camera* m_pCamera;

	bool m_isDebug;
};

