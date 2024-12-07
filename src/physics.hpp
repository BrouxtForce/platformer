#pragma once

#include "scene.hpp"

namespace Physics
{
    struct CollisionData
    {
        bool collided = false;
        Math::float2 contactPoint = 0.0f;
        Math::float2 normal = 0.0f;
        float t = 0.0f;
    };

    struct Line
    {
        Math::float2 from = 0.0f;
        Math::float2 to = 0.0f;
    };

    Math::float2 CollideAndSlide(const Scene& scene, const Transform& ellipse, Math::float2 velocity);

    CollisionData EllipseCast(const Scene& scene, const Transform& ellipse, Math::float2 velocity);

    CollisionData CirclePointCollision(Math::float2 circlePosition, Math::float2 velocity, Math::float2 point);
    CollisionData CircleLineCollision(Math::float2 circlePosition, Math::float2 velocity, const Line& line);
    CollisionData EllipseRectCollision(Transform ellipse, Math::float2 velocity, Transform rect);
}
