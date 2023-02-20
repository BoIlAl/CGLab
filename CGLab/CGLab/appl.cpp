#include "appl.h"
#include "renderer.h"
#include "camera.h"

Appl* Appl::CreateAppl(HWND hWnd)
{
	Appl* appl = new Appl(hWnd);
	if (!appl || !appl->m_pRenderer)
	{
		return nullptr;
	}
	return appl;
}

void Appl::DeleteAppl(Appl*& pAppl)
{
	if (pAppl)
	{
		delete pAppl;
		pAppl = nullptr;
	}
}

bool Appl::Resize(UINT newWidth, UINT newHeight)
{
	return m_pRenderer->Resize(newWidth, newHeight);;
}

void Appl::Render()
{
	m_pRenderer->Render();
}

void Appl::VerticalArrowHandle(bool isUpArrow)
{
	if (isUpArrow)
		m_pRenderer->getCamera()->MoveVertical(deltaMovement);
	else
		m_pRenderer->getCamera()->MoveVertical(-deltaMovement);
	
}

void Appl::HorizontalArrowHandle(bool isLeftArrow)
{
	if(isLeftArrow)
		m_pRenderer->getCamera()->MoveHorizontal(deltaMovement);
	else
		m_pRenderer->getCamera()->MoveHorizontal(-deltaMovement);
}

void Appl::MouseLButtonPressHandle(int x, int y)
{
	m_isPressed = true;
	m_xMouse = x;
	m_yMouse = y;
}

void Appl::MouseMovementHandle(int x, int y)
{
	if (!m_isPressed)
		return;
	m_pRenderer->getCamera()->Rotate(deltaRotate*(float)(x - m_xMouse), deltaRotate * (float)(y - m_yMouse));
	m_xMouse = x;
	m_yMouse = y;
}

void Appl::MouseLButtonUpHandle(int x, int y)
{
	m_isPressed = false;
}


Appl::Appl(HWND hWnd) 
	: m_pRenderer(nullptr, std::mem_fn(&Renderer::Release))
	, m_isPressed(false)
	, m_xMouse(0)
	, m_yMouse(0)
{
	auto renderer = Renderer::CreateRenderer(hWnd);
	if (!renderer) {
		return;
	}
	m_pRenderer.reset(renderer);
}
	
Appl::~Appl()
{
}
