#pragma once
#include "framework.h"

class Renderer;

class Appl {
public:
	static Appl* CreateAppl(HWND hWnd);
	static void DeleteAppl(Appl*& pAppl);

	bool Resize(UINT newWidth, UINT newHeight);
	bool KeypressHandle(WPARAM wParam);
	void Render();
private:
	Appl(HWND hWnd);
	~Appl();
private:
	std::unique_ptr<Renderer, std::_Mem_fn<void(Renderer::*)()>> m_pRenderer;
};