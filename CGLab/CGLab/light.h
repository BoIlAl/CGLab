#pragma once

#include "framework.h"

#include <DirectXMath.h>


class PointLight
{
public:
	PointLight() = default;
	PointLight(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& color, FLOAT brightnessScaleFactor = 1.0f);
	~PointLight() = default;

	void SetPosition(const DirectX::XMFLOAT4& position);
	void SetColor(const DirectX::XMFLOAT4& color);
	void SetBrightness(FLOAT scaleFactor);

private:
	DirectX::XMFLOAT4 m_position;
	DirectX::XMFLOAT4 m_color;
	DirectX::XMFLOAT4 m_brightnessScaleFactor; // r
};
