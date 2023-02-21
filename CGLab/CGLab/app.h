#pragma once
#include "framework.h"

class Renderer;


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
	void MouseLButtonPressHandle(int x, int y);
	void MouseMovementHandle(int x, int y);
	void MouseLButtonUpHandle(int x, int y);

	void NextLightBrightness();

private:
	App(HWND hWnd);
	~App();
private:
	int m_xMouse, m_yMouse;
	bool m_isPressed;

	static constexpr float m_deltaMovement = 0.2f;
	static constexpr float m_deltaRotate = 0.002f;
	static constexpr float m_deltaZoom = 0.002f;

	std::unique_ptr<Renderer, std::_Mem_fn<void(Renderer::*)()>> m_pRenderer;
};