#pragma once

#include <DirectXMath.h>

class Camera
{
public:
    Camera();
    ~Camera();

    DirectX::XMVECTOR GetPosition() const { return m_eye; };
    DirectX::XMVECTOR GetDirection() const { return m_viewDir; };
    DirectX::XMVECTOR GetUp() const { return m_up; };
    DirectX::XMMATRIX GetViewMatrix() const;

    void MoveVertical(float delta);
    void MoveHorizontal(float delta);
    void Rotate(float horisontalAngle, float verticalAngle);

private:

private:
    DirectX::XMVECTOR m_eye;
    DirectX::XMVECTOR m_viewDir;
    DirectX::XMVECTOR m_up;
};