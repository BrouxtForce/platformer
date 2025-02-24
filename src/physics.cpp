#include "physics.hpp"
#include "math.hpp"

#include <cassert>

namespace Physics
{
    GravityZoneInfo GetGravity(const Scene& scene, Math::float2 position, Math::float2 currentGravity)
    {
        GravityZoneInfo closestGravityZoneInfo { .active = false };
        uint16_t highestZIndex = 0;

        auto RegisterGravityZoneInfo = [&](const GravityZoneInfo&& info, Entity* entity)
        {
            if (entity->zIndex >= highestZIndex)
            {
                closestGravityZoneInfo = info;
                highestZIndex = entity->zIndex;
            }
        };

        for (Entity& entity : scene.entities)
        {
            if ((entity.flags & (uint16_t)EntityFlags::GravityZone) == 0)
            {
                continue;
            }
            Math::float2 rotatedPosition = entity.transform.position + Math::RotateVector(position - entity.transform.position, -entity.transform.rotation);
            switch (entity.shape)
            {
                case Shape::Rectangle: {
                    Math::float2 min = entity.transform.position - entity.transform.scale;
                    Math::float2 max = entity.transform.position + entity.transform.scale;
                    if (rotatedPosition.x >= min.x && rotatedPosition.x <= max.x &&
                        rotatedPosition.y >= min.y && rotatedPosition.y <= max.y)
                    {
                        Math::float2 direction = Math::Direction(entity.transform.rotation - Math::PI / 2.0f);
                        RegisterGravityZoneInfo({
                            .active = true,
                            .shape = Shape::Rectangle,
                            .direction = direction,
                            .closestEndDirection = direction,
                            .position = entity.transform.position,
                            .radius = entity.transform.scale.x
                        }, &entity);
                    }
                    break;
                }
                case Shape::Ellipse: {
                    Math::float2 zoneCenter = entity.transform.position;
                    float zoneRadius = entity.transform.scale.x;
                    if (Math::Distance(position, zoneCenter) > zoneRadius)
                    {
                        break;
                    }
                    Math::float2 direction = Math::Normalize(zoneCenter - position);
                    if (Math::Dot(currentGravity, direction) < std::acos(M_PI_4))
                    {
                        break;
                    }

                    float angle = std::atan2(-direction.y, -direction.x);
                    if (angle >= entity.gravityZone.minAngle && angle <= entity.gravityZone.maxAngle)
                    {
                        Math::float2 closestEndDirection;
                        if (std::abs(angle - entity.gravityZone.minAngle) < std::abs(angle - entity.gravityZone.maxAngle))
                        {
                            closestEndDirection = -Math::Direction(entity.gravityZone.minAngle);
                        }
                        else
                        {
                            closestEndDirection = -Math::Direction(entity.gravityZone.maxAngle);
                        }
                        RegisterGravityZoneInfo({
                            .active = true,
                            .shape = Shape::Ellipse,
                            .direction = direction,
                            .closestEndDirection = closestEndDirection,
                            .position = zoneCenter,
                            .radius = zoneRadius
                        }, &entity);
                    }
                    break;
                }
            }
        }
        return closestGravityZoneInfo;
    }

    Math::float2 CollideAndSlide(const Scene& scene, const Transform& ellipse, Math::float2 velocity, std::function<bool(CollisionData)> callback)
    {
        constexpr int MAX_ITERATIONS = 5;
        constexpr float SMALL_DISTANCE = 0.001f;

        Math::float2 resultVelocity = 0.0f;
        for (int i = 0; i < MAX_ITERATIONS; i++)
        {
            CollisionData collisionData = EllipseCast(scene, ellipse, velocity);
            if (!collisionData.collided)
            {
                return resultVelocity + velocity;
            }

            Math::float2 movement = Math::Normalize(velocity) * (collisionData.t * Math::Length(velocity) - SMALL_DISTANCE);
            velocity -= movement;
            velocity -= collisionData.normal * Math::Dot(velocity, collisionData.normal);
            resultVelocity += movement;

            if (!callback(collisionData))
            {
                break;
            }
        }

        return resultVelocity;
    }

    CollisionData EllipseCast(const Scene& scene, const Transform& ellipse, Math::float2 velocity, uint16_t entityFlags)
    {
        CollisionData minCollision { .collided = false, .t = INFINITY };
        for (const Entity& entity : scene.entities)
        {
            if ((entity.flags & entityFlags) == 0)
            {
                continue;
            }

            CollisionData collision { .collided = false };
            switch (entity.shape)
            {
                case Shape::Rectangle:
                    collision = EllipseRectCollision(ellipse, velocity, entity.transform);
                    break;
                case Shape::Ellipse:
                    collision = CircleCircleCollision(ellipse, velocity, entity.transform);
                    break;
            }
            if (!collision.collided)
            {
                continue;
            }
            if (collision.t < minCollision.t)
            {
                minCollision = collision;
                minCollision.entity = &entity;
            }
        }
        return minCollision;
    }

    CollisionData CirclePointCollision(Math::float2 circlePosition, Math::float2 velocity, Math::float2 point)
    {
        float a = Math::LengthSquared(velocity);
        float b = Math::Dot(2 * velocity, circlePosition - point);
        float c = Math::LengthSquared(circlePosition - point) - 1.0f;

        Math::float2 solutions;
        if (!Math::SolveQuadratic(a, b, c, solutions))
        {
            return { .collided = false };
        }

        float t = INFINITY;
        for (int i = 0; i < 2; i++)
        {
            if (solutions[i] >= 0 && solutions[i] <= 1)
            {
                t = std::min(t, solutions[i]);
            }
        }

        if (t == INFINITY)
        {
            return CollisionData { .collided = false };
        }
        assert(t >= 0 && t <= 1);
        return CollisionData {
            .collided = true,
            .contactPoint = point,
            .normal = Math::Normalize(circlePosition + velocity * t - point),
            .t = t
        };
    }

    CollisionData CircleLineCollision(Math::float2 circlePosition, Math::float2 velocity, const Line& line)
    {
        Math::float2 edge = line.to - line.from;
        Math::float2 baseToVertex = line.from - circlePosition;

        float edgeLengthSq = Math::LengthSquared(edge);
        float edgeDotVelocity = Math::Dot(edge, velocity);
        float edgeDotBaseToVertex = Math::Dot(edge, baseToVertex);

        float a = edgeLengthSq * -Math::LengthSquared(velocity) + edgeDotVelocity * edgeDotVelocity;
        float b = edgeLengthSq * 2.0f * Math::Dot(velocity, baseToVertex) - 2.0f * edgeDotVelocity * edgeDotBaseToVertex;
        float c = edgeLengthSq * (1.0f - Math::LengthSquared(baseToVertex)) + edgeDotBaseToVertex * edgeDotBaseToVertex;

        Math::float2 solutions;
        if (!Math::SolveQuadratic(a, b, c, solutions))
        {
            return { .collided = false };
        }

        Math::float2 intersectionPoint;

        float t = INFINITY;
        for (int i = 0; i < 2; i++)
        {
            float f = (edgeDotVelocity * solutions[i] - edgeDotBaseToVertex) / edgeLengthSq;
            if (solutions[i] >= 0 && solutions[i] <= 1 && f >= 0 && f <= 1 && solutions[i] < t)
            {
                t = solutions[i];
                intersectionPoint = line.from + f * edge;
            }
        }

        if (t == INFINITY)
        {
            return CollisionData { .collided = false };
        }
        assert(t >= 0 && t <= 1);
        return CollisionData {
            .collided = true,
            .contactPoint = intersectionPoint,
            .normal = Math::Normalize(circlePosition - intersectionPoint),
            .t = t
        };
    }

    CollisionData EllipseRectCollision(Transform ellipse, Math::float2 velocity, Transform rect)
    {
        ellipse.position /= ellipse.scale;
        velocity /= ellipse.scale;
        rect.position /= ellipse.scale;
        rect.scale /= ellipse.scale;

        Math::Complex rectRotation = Math::Complex::FromAngle(rect.rotation);
        std::array<Math::float2, 4> corners {
            rect.position + rectRotation * -rect.scale,
            rect.position + rectRotation * Math::float2(rect.scale.x, -rect.scale.y),
            rect.position + rectRotation * rect.scale,
            rect.position + rectRotation * Math::float2(-rect.scale.x, rect.scale.y)
        };

        CollisionData minCollision { .collided = false, .t = INFINITY };
        for (int i = 0; i < (int)corners.size(); i++)
        {
            Math::float2 corner = corners[i];
            Math::float2 nextCorner = corners[(i + 1) % corners.size()];

            CollisionData collision;
            collision = CirclePointCollision(ellipse.position, velocity, corner);
            if (collision.collided && collision.t < minCollision.t)
            {
                minCollision = collision;
            }

            collision = CircleLineCollision(ellipse.position, velocity, Line{ corner, nextCorner });
            if (collision.collided && collision.t < minCollision.t)
            {
                minCollision = collision;
            }
        }

        minCollision.contactPoint *= ellipse.scale;
        minCollision.normal = Math::Normalize(minCollision.normal / ellipse.scale);

        return minCollision;
    }

    CollisionData CircleCircleCollision(const Transform& circle, Math::float2 velocity, const Transform& other)
    {
        const float circleRadius = circle.scale.x;
        const float otherRadius = other.scale.x;

        float targetDist = circleRadius + otherRadius;

        float a = Math::LengthSquared(velocity);
        float b = Math::Dot(2 * velocity, circle.position - other.position);
        float c = Math::LengthSquared(circle.position - other.position) - targetDist * targetDist;

        Math::float2 solutions;
        if (!Math::SolveQuadratic(a, b, c, solutions))
        {
            return { .collided = false };
        }

        float t = INFINITY;
        for (int i = 0; i < 2; i++)
        {
            if (solutions[i] >= 0.0f && solutions[i] <= 1.0f)
            {
                t = std::min(solutions[i], t);
            }
        }

        if (t == INFINITY)
        {
            return { .collided = false };
        }

        Math::float2 normal = Math::Normalize(circle.position + velocity * t - other.position);
        return CollisionData {
            .collided = true,
            .contactPoint = other.position - normal * otherRadius,
            .normal = normal,
            .t = t
        };
    }
}
