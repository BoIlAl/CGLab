#include "appl.h"
#include "renderer.h"

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

bool Appl::KeypressHandle(WPARAM wParam)
{
	switch (wParam)
	{
	default:
		{
			return false;
		}
	}
}

void Appl::Render()
{
	m_pRenderer->Render();
}

Appl::Appl(HWND hWnd) : m_pRenderer(nullptr, std::mem_fn(&Renderer::Release))
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
