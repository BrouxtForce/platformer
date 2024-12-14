#pragma once

#include "math.hpp"
#include "scene.hpp"

class Player
{
public:
    Player() = default;
    Player(Entity* entity);

    void Move(const Scene& scene, Math::float2 input);

private:
    Math::float2 m_Velocity{};
    Math::float2 m_GravityVelocity{};

    Math::float2 m_GravityDirection = Math::float2(0, -1.0f);

    Entity* m_Entity = nullptr;

    bool m_IsOnGround = false;
};
