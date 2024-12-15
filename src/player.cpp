#include "player.hpp"
#include "physics.hpp"

Player::Player(Entity* entity)
    : m_Entity(entity) {}

void Player::Move(const Scene& scene, Math::float2 input)
{
    std::optional<Math::float2> gravity = Physics::GetGravity(scene, m_Entity->transform.position);
    if (gravity.has_value())
    {
        gravityDirection = gravity.value();
    }

    constexpr float acceleration = 0.03f;
    constexpr float drag = 0.50f;
    velocity += Math::float2(-gravityDirection.y, gravityDirection.x) * input.x * acceleration;
    velocity *= drag;

    m_Entity->transform.position += Physics::CollideAndSlide(
        scene, m_Entity->transform, velocity,
        [&](Physics::CollisionData collisionData)
        {
            velocity -= collisionData.normal * Math::Dot(collisionData.normal, velocity);
            return true;
        }
    );

    constexpr float gravityAcceleration = 0.002f;
    gravityVelocity += gravityDirection * gravityAcceleration;

    if (m_IsOnGround && input.y > 0.0f)
    {
        constexpr float jumpAcceleration = 0.03f;
        gravityVelocity = -gravityDirection * jumpAcceleration;
    }

    m_IsOnGround = false;
    m_Entity->transform.position += Physics::CollideAndSlide(
        scene, m_Entity->transform, gravityVelocity,
        [&](Physics::CollisionData collisionData)
        {
            if (Math::Dot(collisionData.normal, -gravityDirection) >= std::acos(M_PI_4))
            {
                m_IsOnGround = true;
                gravityVelocity = 0.0f;
                return false;
            }
            gravityVelocity -= collisionData.normal * Math::Dot(collisionData.normal, gravityVelocity);
            return true;
        }
    );
}
