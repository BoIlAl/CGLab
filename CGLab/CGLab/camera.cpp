#include "framework.h"

#include "Camera.h"

Camera::Camera() :
	m_eye(DirectX::XMVectorSet(0.0f, 4.0f, -15.0f, 1.0f)),
	m_viewDir(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)),
	m_up(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))
{
};


DirectX::XMMATRIX Camera::GetViewMatrix() const
{
	return DirectX::XMMatrixLookToLH(
		m_eye,
		m_viewDir,
		m_up);
}

void Camera::MoveVertical(float delta)
{
	m_eye = DirectX::XMVectorAdd(m_eye, DirectX::XMVectorSet(0, 0, delta, 0));
}

void Camera::MoveHorizontal(float delta)
{
	m_eye = DirectX::XMVectorAdd(m_eye, DirectX::XMVectorSet(-delta, 0, 0, 0));
}

void Camera::Rotate(float horisontalAngle, float verticalAngle)
{
	m_viewDir = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(m_viewDir, DirectX::XMVectorSet(horisontalAngle, verticalAngle, 0, 0)));
}

Camera::~Camera()
{}
