#include "light.h"


PointLight::PointLight(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& color, FLOAT brightnessScaleFactor)
	: m_position(position.x, position.y, position.z, 1.0f)
	, m_color(color)
	, m_brightnessScaleFactor({ brightnessScaleFactor, 0.0f, 0.0f, 0.0f })
{}


void PointLight::SetPosition(const DirectX::XMFLOAT4& position)
{
	m_position = position;
}

void PointLight::SetColor(const DirectX::XMFLOAT4& color)
{
	m_color = color;
}

void PointLight::SetBrightness(FLOAT scaleFactor)
{
	m_brightnessScaleFactor = { scaleFactor, 0.0f, 0.0f, 0.0f };
}


DirectionalLight::DirectionalLight(const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT4 color)
	: m_direction(direction.x, direction.y, direction.z, 1.0f)
	, m_color(color)
{
	for (UINT i = 0; i < 4; ++i)
	{
		DirectX::XMStoreFloat4x4(&m_vpMatrix[i], DirectX::XMMatrixIdentity());
	}
}

void DirectionalLight::SetDirection(const DirectX::XMFLOAT3& direction)
{
	m_direction.x = direction.x;
	m_direction.y = direction.y;
	m_direction.z = direction.z;
	m_direction.w = 1.0f;
}

void DirectionalLight::SetColor(const DirectX::XMFLOAT4& color)
{
	m_color = color;
}

void DirectionalLight::SetVpMatrix(UINT splitIdx, const DirectX::XMMATRIX& vpMatrix)
{
	assert(splitIdx < 4);

	DirectX::XMStoreFloat4x4(&m_vpMatrix[splitIdx], vpMatrix);
}
