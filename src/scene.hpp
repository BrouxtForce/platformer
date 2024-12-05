#pragma once

#include <vector>

#include "transform.hpp"

struct Material
{
    Math::Color color;
};

struct Entity
{
    uint16_t id = 0;
    Transform transform{};
    Material material{};
};

class Scene
{
public:
    std::vector<Entity> entities;

    Scene() = default;

    int CreateEntity();
};
