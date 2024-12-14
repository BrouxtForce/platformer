#include "player.hpp"
#include "physics.hpp"

Player::Player(Entity* entity)
    : m_Entity(entity) {}

void Player::Move(const Scene& scene, Math::float2 input)
{
    std::optional<Math::float2> gravity = Physics::GetGravity(scene, m_Entity->transform.position);
    if (gravity.has_value())
    {
        m_GravityDirection = gravity.value();
    }

    constexpr float acceleration = 0.03f;
    constexpr float drag = 0.50f;
    m_Velocity += Math::float2(-m_GravityDirection.y, m_GravityDirection.x) * input.x * acceleration;
    m_Velocity *= drag;

    m_Entity->transform.position += Physics::CollideAndSlide(
        scene, m_Entity->transform, m_Velocity,
        [&](Physics::CollisionData collisionData)
        {
            m_Velocity -= collisionData.normal * Math::Dot(collisionData.normal, m_Velocity);
            return true;
        }
    );

    constexpr float gravityAcceleration = 0.002f;
    m_GravityVelocity += m_GravityDirection * gravityAcceleration;

    if (m_IsOnGround && input.y > 0.0f)
    {
        constexpr float jumpAcceleration = 0.03f;
        m_GravityVelocity = -m_GravityDirection * jumpAcceleration;
    }

    m_IsOnGround = false;
    m_Entity->transform.position += Physics::CollideAndSlide(
        scene, m_Entity->transform, m_GravityVelocity,
        [&](Physics::CollisionData collisionData)
        {
            if (Math::Dot(collisionData.normal, -m_GravityDirection) >= std::acos(M_PI_4))
            {
                m_IsOnGround = true;
                m_GravityVelocity = 0.0f;
                return false;
            }
            m_GravityVelocity -= collisionData.normal * Math::Dot(collisionData.normal, m_GravityVelocity);
            return true;
        }
    );
}
