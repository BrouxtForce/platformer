#pragma once

#include "math.hpp"
#include "scene.hpp"

class Player
{
public:
    Math::float2 velocity{};
    Math::float2 gravityVelocity{};
    Math::float2 gravityDirection = Math::float2(0, -1.0f);

    Player() = default;
    Player(Entity* entity);

    void Move(const Scene& scene, Math::float2 input);

    inline Transform& GetTransform() {
        return m_Entity->transform;
    }

private:
    Entity* m_Entity = nullptr;

    bool m_IsOnGround = false;
};
