#pragma once

#include "framework.h"

struct IDXGIFactory;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11RasterizerState;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;

class ShaderCompiler;


class Renderer
{
public:
	static Renderer* CreateRenderer(HWND hWnd);
	static void DeleterRenderer(Renderer*& pRenderer);

	void Release();

	~Renderer();

	void Render();

private:
	Renderer();

	bool Init(HWND hWnd);

	HRESULT CreateDevice(IDXGIFactory* pFactory);
	HRESULT CreateSwapChain(IDXGIFactory* pFactory, HWND hWnd);
	HRESULT CreateBackBuffer();
	HRESULT CreateRasterizerState();
	HRESULT CreateSceneResources();

	void Update();
	void RenderScene();

private:
	static constexpr FLOAT s_near = 0.001f;
	static constexpr FLOAT s_far = 1000.0f;
	static constexpr FLOAT s_fov = 3.14159265359f * 2.0f / 3.0f;

	static constexpr FLOAT s_cameraPosition[3] = { 0.0f, 0.0f, -5.0f };

private:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

	IDXGISwapChain* m_pSwapChain;
	ID3D11RenderTargetView* m_pBackBufferRTV;

	ID3D11RasterizerState* m_pRasterizerState;

	ID3D11Buffer* m_pVertexBuffer;
	ID3D11Buffer* m_pIndexBuffer;
	UINT m_indexCount;

	ID3D11Buffer* m_pConstantBuffer;

	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader* m_pPixelShader;

	ID3D11InputLayout* m_pInputLayout;

	UINT m_windowWidth;
	UINT m_windowHeight;

	ShaderCompiler* m_pShaderCompiler;
};

