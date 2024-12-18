#include "player.hpp"
#include "physics.hpp"

Player::Player(Entity* entity)
    : m_Entity(entity) {}

void Player::Move(const Scene& scene, Math::float2 input, bool jump)
{
    std::optional<Math::float2> gravity = Physics::GetGravity(scene, m_Entity->transform.position, gravityDirection, &m_ClosestEndDirection);
    if (gravity.has_value())
    {
        gravityDirection = gravity.value();
        m_ClosestDirectionUsed = false;
    }
    else if (!m_ClosestDirectionUsed)
    {
        gravityDirection = m_ClosestEndDirection;
        m_ClosestDirectionUsed = true;
    }

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

    gravityVelocity += gravityDirection * gravityAcceleration;

    m_JumpFrames++;
    if (jump)
    {
        m_JumpFrames = 0;
    }
    if (m_IsOnGround)
    {
        if (m_JumpFrames < s_MaxJumpFrames)
        {
            Jump();
        }
        else
        {
            m_CoyoteFrames = 0;
        }
    }
    if (m_CoyoteFrames < s_MaxCoyoteFrames)
    {
        m_CoyoteFrames++;
        if (m_JumpFrames == 0)
        {
            Jump();
        }
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

void Player::Jump()
{
    gravityVelocity = -gravityDirection * jumpAcceleration;
    m_JumpFrames = s_MaxJumpFrames;
    m_CoyoteFrames = s_MaxCoyoteFrames;
}
