#include "camera.hpp"

Math::Matrix3x3 Camera::GetMatrix() const
{
    Math::float2 s = 1.0f / transform.scale;
    Math::float2 t = -transform.position;
    const float cosx = std::cos(-transform.rotation);
    const float sinx = std::sin(-transform.rotation);

    // Apply translation, then rotation, then scale
    return Math::Matrix3x3(
        Math::float3( s.x * cosx / aspect,                  s.y * sinx,                  0),
        Math::float3(-s.x * sinx / aspect,                  s.y * cosx,                  0),
        Math::float3( s.x * (t.x*cosx - t.y*sinx) / aspect, s.y * (t.x*sinx + t.y*cosx), 1)
    );
}

void Camera::FollowPlayer(Math::float2 position, Math::float2 offset, Math::float2 down, float deltaTime)
{
    constexpr float rotationHalfLifeSeconds = 0.05f;
    constexpr float positionHalfLifeSeconds = 0.15f;

    float targetRotation = std::atan2f(down.y, down.x) + Math::PI / 2.0f;
    transform.rotation = Math::LerpSmooth(transform.rotation, targetRotation, deltaTime, rotationHalfLifeSeconds);

    Math::float2 targetPosition = position + Math::RotateVector(offset, transform.rotation);
    transform.position = Math::LerpSmooth(transform.position, targetPosition, deltaTime, positionHalfLifeSeconds);
}
