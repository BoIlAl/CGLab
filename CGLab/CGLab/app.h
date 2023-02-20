#pragma once
#include "framework.h"

class Renderer;

class App {
public:
	static App* CreateAppl(HWND hWnd);
	static void DeleteAppl(App*& pAppl);

	bool Resize(UINT newWidth, UINT newHeight);
	void Render();

	void VerticalArrowHandle(bool isUpArrow);
	void HorizontalArrowHandle(bool isLeftArrow);

	void MouseLButtonPressHandle(int x, int y);
	void MouseMovementHandle(int x, int y);
	void MouseLButtonUpHandle(int x, int y);
private:
	App(HWND hWnd);
	~App();
private:
	int m_xMouse, m_yMouse;
	bool m_isPressed;
	const float deltaMovement = 0.2f;
	const float deltaRotate = 0.002f;
	std::unique_ptr<Renderer, std::_Mem_fn<void(Renderer::*)()>> m_pRenderer;
};