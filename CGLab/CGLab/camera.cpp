#include "framework.h"

#include "Camera.h"


Camera::Camera() : m_eye(DirectX::XMVectorSet(0.0f, 1.0f, -15.0f, 1.0f)),
m_viewDir(DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f))),
m_up(DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))),
m_vertAngle(0.0f),
m_horzAngle(0.0f)
{
	Update();
}

void Camera::MoveVertical(float delta)
{
	m_eye = DirectX::XMVectorAdd(m_eye, DirectX::XMVectorScale(CalcProjectedDir(), delta));
}

void Camera::MoveHorizontal(float delta)
{
	DirectX::FXMVECTOR right = DirectX::XMVector3Rotate(CalcProjectedDir(),
														DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 1, 0, 1), 
													   -DirectX::XM_PIDIV2));
	m_eye = DirectX::XMVectorAdd(m_eye, DirectX::XMVectorScale(right, delta));
}

void Camera::MovePerpendicular(float delta)
{
	m_eye = DirectX::XMVectorAdd(m_eye, DirectX::XMVectorSet(0, delta, 0, 1));
}

void Camera::Rotate(float horisontalAngle, float verticalAngle)
{
	m_vertAngle += verticalAngle;
	m_horzAngle -= horisontalAngle;

	Update();
}

void Camera::Zoom(float delta)
{
	m_eye = DirectX::XMVectorAdd(m_eye, DirectX::XMVectorScale(m_viewDir, delta));
	Update();
}

void Camera::Update()
{
	if (m_vertAngle > DirectX::XM_PIDIV2)
	{
		m_vertAngle = DirectX::XM_PIDIV2;
	}
	if (m_vertAngle < -DirectX::XM_PIDIV2)
	{
		m_vertAngle = -DirectX::XM_PIDIV2;
	}

	if (m_horzAngle >= DirectX::XM_2PI)
	{
		m_horzAngle -= DirectX::XM_2PI;
	}
	if (m_horzAngle < 0)
	{
		m_horzAngle += DirectX::XM_2PI;
	}

	static const DirectX::FXMVECTOR initRight = DirectX::XMVectorSet(1, 0, 0, 1);
	static const DirectX::FXMVECTOR initUp = DirectX::XMVectorSet(0, 1, 0, 1);
	static const DirectX::FXMVECTOR initDir = DirectX::XMVectorSet(0, 0, 1, 1);

	m_viewDir = DirectX::XMVector3Rotate(initDir, DirectX::XMQuaternionRotationAxis(initRight, m_vertAngle));
	m_viewDir = DirectX::XMVector3Rotate(m_viewDir, DirectX::XMQuaternionRotationAxis(initUp, m_horzAngle));

	m_up = DirectX::XMVector3Rotate(initUp, DirectX::XMQuaternionRotationAxis(initRight, m_vertAngle));
	m_up = DirectX::XMVector3Rotate(m_up, DirectX::XMQuaternionRotationAxis(initUp, m_horzAngle));
}

DirectX::FXMVECTOR Camera::CalcProjectedDir() const
{
	DirectX::FXMVECTOR dir = DirectX::XMVector3Normalize(DirectX::XMVectorSet(
		DirectX::XMVectorGetX(m_viewDir),
		0.0f,
		DirectX::XMVectorGetZ(m_viewDir),
		1.0f
	));

	return dir;
}

DirectX::XMMATRIX Camera::GetViewMatrix() const
{
	return DirectX::XMMatrixLookToLH(m_eye, m_viewDir, m_up);
}

Camera::~Camera()
{}