#include "framework.h"

#include "Camera.h"

Camera::Camera() :
	m_eye(DirectX::XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f)),
	m_viewDir(DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f))),
	m_up(DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)))
{
};


DirectX::XMMATRIX Camera::GetViewMatrix() const
{
	return DirectX::XMMatrixLookAtLH(
		{ 0.0f, 5.0f, -15.0f , 1.0f },
		{ 0.0f, 2.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f });
	/*return DirectX::XMMatrixLookAtLH(
		m_eye,
		m_viewDir,
		m_up);*/
}

Camera::~Camera()
{}
