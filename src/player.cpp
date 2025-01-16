#include "player.hpp"
#include "physics.hpp"

Player::Player(Entity* entity)
    : m_Entity(entity) {}

void Player::Move(const Scene& scene, float cameraRotation, const Input& input)
{
    Math::float2 prevGravityDirection = gravityDirection;
    Transform prevTransform = m_Entity->transform;

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

    float velocityRotationAngle = -std::acos(std::clamp(Math::Dot(prevGravityDirection, gravityDirection), -1.0f, 1.0f));
    if (velocityRotationAngle > -Math::PI / 4)
    {
        if (Math::Dot(prevGravityDirection, { -gravityDirection.y, gravityDirection.x }) < 0.0f)
        {
            velocityRotationAngle = -velocityRotationAngle;
        }
        velocity = Math::RotateVector(velocity, velocityRotationAngle);
    }

    Math::float2 cameraRight = Math::Direction(cameraRotation);
    Math::float2 cameraDown = { cameraRight.y, -cameraRight.x };

    Math::float2 movement = 0.0f;

    float gravityDotDown = Math::Dot(gravityDirection, cameraDown);
    if (std::abs(gravityDotDown) < Math::SQRT_2)
    {
        movement = Math::float2{ std::abs(gravityDirection.y), gravityDirection.x } * input.Joystick().x;
    }
    else
    {
        movement = Math::float2{ -gravityDirection.y, gravityDirection.x } * input.Joystick().x;
    }

    if (input.IsKeyDown(Key::Boost))
    {
        velocity += movement * boostAcceleration;
    }
    else
    {
        velocity += movement * acceleration;
        velocity *= drag;
    }

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
    if (input.IsKeyPressed(Key::Jump))
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

    if (Physics::EllipseCast(scene, prevTransform, m_Entity->transform.position - prevTransform.position, (uint16_t)EntityFlags::DeathZone).collided)
    {
        m_Entity->transform.position = spawnPoint;
        velocity = 0.0f;
        gravityVelocity = 0.0f;
        gravityDirection = { 0.0f, -1.0f };
    }
}

void Player::Jump()
{
    gravityVelocity = -gravityDirection * jumpAcceleration;
    m_JumpFrames = s_MaxJumpFrames;
    m_CoyoteFrames = s_MaxCoyoteFrames;
}
