#pragma once

#include "math.hpp"

#include "physics.hpp"
#include "scene.hpp"
#include "input.hpp"

class Player
{
public:
    float acceleration = 0.035f;
    float jumpAcceleration = 0.04f;
    float gravityAcceleration = 0.003f;
    float drag = 0.50f;

    float boostAcceleration = 0.001f;

    Math::float2 velocity{};
    Math::float2 gravityVelocity{};
    Math::float2 gravityDirection = Math::float2(0, -1.0f);

    Math::float2 spawnPoint{};

    Player() = default;
    Player(Entity* entity);

    void Update(const Scene& scene, float cameraRotation, float currentTime, const Input& input, bool* finishedLevel);
    void Jump();

    inline Transform& GetTransform() {
        return m_Entity->transform;
    }

private:
    Entity* m_Entity = nullptr;

    bool m_IsOnGround = false;

    Physics::GravityZoneInfo m_PrevGravityZoneInfo{};

    static constexpr int s_MaxCoyoteFrames = 5;
    int m_CoyoteFrames = s_MaxCoyoteFrames + 1;

    static constexpr int s_MaxJumpFrames = 5;
    int m_JumpFrames = s_MaxCoyoteFrames + 1;

    float m_PrevHorizontalInput = 0.0f;
    bool m_ShouldFlipInput = false;
};
