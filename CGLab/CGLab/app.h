#pragma once
#include "framework.h"
#include "renderer.h"
#include "camera.h"

class App
{
public:
	static constexpr UINT MaxWindowSize = 2048u;

public:
	static App* CreateAppl(HWND hWnd);
	static void DeleteAppl(App*& pAppl);

	bool Resize(UINT newWidth, UINT newHeight);
	void Render();

	void VerticalArrowHandle(bool isUpArrow);
	void HorizontalArrowHandle(bool isLeftArrow);
	void XHandle();
	void ZHandle();
	
	void MouseWheel(int delta);
	void MouseRButtonPressHandle(int x, int y);
	void MouseMovementHandle(int x, int y);
	void MouseRButtonUpHandle(int x, int y);

	void AddButtonHandle();

private:
	App(HWND hWnd);
	~App();
private:
	int m_xMouse, m_yMouse;
	bool m_isPressed;

	static constexpr float m_deltaMovement = 0.2f;
	static constexpr float m_deltaRotate = 0.001f;
	static constexpr float m_deltaZoom = 0.002f;

	struct Deleter
	{
		void operator()(Renderer*& pRenderer)
		{
			Renderer::DeleteRenderer(pRenderer);
		}
	};

	std::unique_ptr<Renderer, Deleter> m_pRenderer;
};