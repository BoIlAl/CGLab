#pragma once

#include <DirectXMath.h>

class Camera 
{
public:
    Camera();
    ~Camera();

    DirectX::XMMATRIX GetViewMatrix() const;

    void MoveVertical(float delta);
    void MoveHorizontal(float delta);
    void MovePerpendicular(float delta);
    void Rotate(float zenith, float azim);
    void Zoom(float delta);

private:
    void Update();
    DirectX::FXMVECTOR CalcProjectedDir() const;

private:
    DirectX::XMVECTOR m_eye;
    DirectX::XMVECTOR m_viewDir;
    DirectX::XMVECTOR m_up;

    float m_vertAngle;
    float m_horzAngle;
};