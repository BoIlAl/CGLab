#pragma once

#include "framework.h"
#include "common.h"

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


class DirectionalLight
{
public:
	DirectionalLight() = default;
	DirectionalLight(const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT4 color);
	~DirectionalLight() = default;

	void SetDirection(const DirectX::XMFLOAT3& direction);
	void SetColor(const DirectX::XMFLOAT4& color);
	void SetVpMatrix(UINT splitIdx, const DirectX::XMMATRIX& vpMatrix);

	inline DirectX::XMFLOAT3 GetDirection() const { return { m_direction.x, m_direction.y, m_direction.z }; }
	inline DirectX::XMFLOAT4X4 GetVpMatrix(UINT splitIdx) const { return m_vpMatrix[splitIdx]; }

private:
	DirectX::XMFLOAT4 m_direction;
	DirectX::XMFLOAT4 m_color;

	DirectX::XMFLOAT4X4 m_vpMatrix[PSSMMaxSplitsNum];
};
