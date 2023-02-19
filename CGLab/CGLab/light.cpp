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
