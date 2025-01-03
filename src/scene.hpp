#pragma once

#include <vector>
#include <memory>

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
    None        = 0,
    Collider    = 1 << 0,
    GravityZone = 1 << 1,
    Text        = 1 << 2,
    Hidden      = 1 << 3
};

struct GravityZone
{
    float minAngle = -M_PI;
    float maxAngle = M_PI;
};

struct Entity
{
    std::string name;
    uint16_t id = 1;
    uint16_t flags = 0;
    uint16_t zIndex = 0;
    Transform transform{};
    Material material{};
    Shape shape = Shape::Rectangle;
    GravityZone gravityZone{};
};

class Scene
{
public:
    std::vector<std::unique_ptr<Entity>> entities;

    struct Properties
    {
        Math::Color backgroundColor;
    };

    Properties properties;

    Scene() = default;

    Entity* CreateEntity();

    constexpr static int City = 0;
    void Load(int index);
};
