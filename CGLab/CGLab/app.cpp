#include "app.h"
#include "renderer.h"
#include "camera.h"

App* App::CreateAppl(HWND hWnd)
{
	App* appl = new App(hWnd);
	if (!appl || !appl->m_pRenderer)
	{
		return nullptr;
	}
	return appl;
}

void App::DeleteAppl(App*& pAppl)
{
	if (pAppl)
	{
		delete pAppl;
		pAppl = nullptr;
	}
}

bool App::Resize(UINT newWidth, UINT newHeight)
{
	return m_pRenderer->Resize(newWidth, newHeight);;
}

void App::Render()
{
	m_pRenderer->Render();
}

void App::VerticalArrowHandle(bool isUpArrow)
{
	if (isUpArrow)
		m_pRenderer->getCamera()->MoveVertical(m_deltaMovement);
	else
		m_pRenderer->getCamera()->MoveVertical(-m_deltaMovement);
	
}

void App::HorizontalArrowHandle(bool isLeftArrow)
{
	if(isLeftArrow)
		m_pRenderer->getCamera()->MoveHorizontal(m_deltaMovement);
	else
		m_pRenderer->getCamera()->MoveHorizontal(-m_deltaMovement);
}

void App::XHandle()
{
	m_pRenderer->getCamera()->MovePerpendicular(m_deltaMovement);
}

void App::ZHandle()
{
	m_pRenderer->getCamera()->MovePerpendicular(-m_deltaMovement);
}

void App::MouseWheel(int delta)
{
	m_pRenderer->getCamera()->Zoom(m_deltaZoom * (float)delta);
}

void App::MouseLButtonPressHandle(int x, int y)
{
	m_isPressed = true;
	m_xMouse = x;
	m_yMouse = y;
}

void App::MouseMovementHandle(int x, int y)
{
	if (!m_isPressed)
		return;
	m_pRenderer->getCamera()->Rotate(m_deltaRotate*(float)(x - m_xMouse), m_deltaRotate * (float)(y - m_yMouse));
	m_xMouse = x;
	m_yMouse = y;
}

void App::MouseLButtonUpHandle(int x, int y)
{
	m_isPressed = false;
}

void App::NextLightBrightness()
{
	static constexpr FLOAT brightness[] = { 1.0f, 10.0f, 100.0f };
	static UINT curIdx = 0;

	curIdx = (curIdx + 1u) % (UINT)_countof(brightness);

	m_pRenderer->ChangeLightBrightness(2, brightness[curIdx]);
}


App::App(HWND hWnd) 
	: m_pRenderer(nullptr, std::mem_fn(&Renderer::Release))
	, m_isPressed(false)
	, m_xMouse(0)
	, m_yMouse(0)
{
	auto renderer = Renderer::CreateRenderer(hWnd);
	if (!renderer)
	{
		return;
	}
	m_pRenderer.reset(renderer);
}
	
App::~App()
{
}
