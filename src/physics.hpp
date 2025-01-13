#pragma once

#include <functional>

#include "scene.hpp"

namespace Physics
{
    struct CollisionData
    {
        bool collided = false;
        Math::float2 contactPoint = 0.0f;
        Math::float2 normal = 0.0f;
        float t = 0.0f;
        const Entity* entity = nullptr;
    };

    struct Line
    {
        Math::float2 from = 0.0f;
        Math::float2 to = 0.0f;
    };

    std::optional<Math::float2> GetGravity(const Scene& scene, Math::float2 position, Math::float2 currentGravity, Math::float2* closestEndDirection);

    Math::float2 CollideAndSlide(const Scene& scene, const Transform& ellipse, Math::float2 velocity, std::function<bool(CollisionData)> callback);

    CollisionData EllipseCast(const Scene& scene, const Transform& ellipse, Math::float2 velocity, uint16_t entityFlags = (uint16_t)EntityFlags::Collider);

    CollisionData CirclePointCollision(Math::float2 circlePosition, Math::float2 velocity, Math::float2 point);
    CollisionData CircleLineCollision(Math::float2 circlePosition, Math::float2 velocity, const Line& line);
    CollisionData EllipseRectCollision(Transform ellipse, Math::float2 velocity, Transform rect);
    CollisionData CircleCircleCollision(const Transform& circle, Math::float2 velocity, const Transform& other);
}
