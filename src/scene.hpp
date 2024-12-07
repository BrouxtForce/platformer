#pragma once

#include <vector>
#include <unordered_map>

#include "transform.hpp"

struct Material
{
    Math::Color color;
};

enum class Shape
{
    Rectangle,
    Ellipse
};

enum class EntityFlags : uint16_t
{
    Player   = 1 << 0,
    Collider = 1 << 1
};

struct Entity
{
    uint16_t id = 0;
    uint16_t flags = 0;
    Transform transform{};
    Material material{};
    Shape shape = Shape::Rectangle;
};

class Scene
{
public:
    Scene() = default;

    Entity* CreateEntity();

    inline const std::unordered_map<uint32_t, Entity>& GetEntityMap() const
    {
        return m_EntityMap;
    }

private:
    std::unordered_map<uint32_t, Entity> m_EntityMap;
};
