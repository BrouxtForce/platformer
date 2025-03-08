#include "player.hpp"
#include "physics.hpp"
#include "shader-library.hpp"

Player::Player(Entity* entity)
    : m_Entity(entity) {}

void Player::Update(const Scene& scene, float cameraRotation, float currentTime, const Input& input, bool* finishedLevel)
{
    Math::float2 prevGravityDirection = gravityDirection;
    Transform prevTransform = m_Entity->transform;

    Physics::GravityZoneInfo gravityZoneInfo = Physics::GetGravity(scene, m_Entity->transform.position, gravityDirection);
    if (gravityZoneInfo.active)
    {
        gravityDirection = gravityZoneInfo.direction;
    }
    else if (m_PrevGravityZoneInfo.active && m_PrevGravityZoneInfo.shape == Shape::Ellipse &&
        Math::Distance(m_Entity->transform.position, m_PrevGravityZoneInfo.position) < m_PrevGravityZoneInfo.radius)
    {
        gravityDirection = m_PrevGravityZoneInfo.closestEndDirection;
        m_PrevGravityZoneInfo.active = false;
    }
    m_PrevGravityZoneInfo = gravityZoneInfo;

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

    float horizontalInput = input.Joystick().x;
    bool maintainShouldFlipInput = true;
    if (horizontalInput == 0.0f || m_PrevHorizontalInput == 0.0f ||
        horizontalInput > 0.0f != m_PrevHorizontalInput > 0.0f)
    {
        maintainShouldFlipInput = false;
    }
    if (!gravityZoneInfo.active || gravityZoneInfo.shape != Shape::Ellipse)
    {
        maintainShouldFlipInput = true;
        m_ShouldFlipInput = gravityDirection.y > Math::SQRT_2 / 2.0f;
    }
    Math::float2 movement = Math::float2{ -gravityDirection.y, gravityDirection.x } * horizontalInput;
    if (!maintainShouldFlipInput)
    {
        m_ShouldFlipInput = movement.x >= 0.0f != horizontalInput >= 0.0f;
    }
    if (m_ShouldFlipInput)
    {
        movement = -movement;
    }
    m_PrevHorizontalInput = horizontalInput;

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

    // TODO: Fix
    /*
    Physics::CollisionData checkpointCollision = Physics::EllipseCast(scene, prevTransform, m_Entity->transform.position - prevTransform.position, (uint16_t)EntityFlags::Checkpoint);
    if (checkpointCollision.collided && !GetUniform<uint32_t>(checkpointCollision.entity, "started").value_or(1))
    {
        // Cry about it
        Entity* checkpointEntity = const_cast<Entity*>(checkpointCollision.entity);
        spawnPoint = checkpointEntity->transform.position;
        if (checkpointEntity->shader)
        {
            WriteUniform<uint32_t>(checkpointEntity, "started", 1);
            WriteUniform<float>(checkpointEntity, "start_time", currentTime);
        }
    }

    Transform prevTransformPoint = prevTransform;
    prevTransformPoint.scale = 0.001f;
    Physics::CollisionData exitCollision = Physics::EllipseCast(scene, prevTransformPoint, m_Entity->transform.position - prevTransform.position, (uint16_t)EntityFlags::Exit);
    if (exitCollision.collided && !GetUniform<uint32_t>(exitCollision.entity, "started").value_or(1))
    {
        // Cry about it
        Entity* exitEntity = const_cast<Entity*>(exitCollision.entity);
        *finishedLevel = true;
        if (exitEntity->shader)
        {
            WriteUniform<uint32_t>(exitEntity, "started", 1);
            WriteUniform<float>(exitEntity, "start_time", currentTime);
        }
    }
    */
}

void Player::Jump()
{
    gravityVelocity = -gravityDirection * jumpAcceleration;
    m_JumpFrames = s_MaxJumpFrames;
    m_CoyoteFrames = s_MaxCoyoteFrames;
}
