#pragma once

#include <vector>
#include <memory>
#include <sstream>

#include "transform.hpp"

class Shader;
struct Material
{
    static constexpr size_t SIZE = 64;
    std::array<uint32_t, SIZE / 4> data;

    void WriteColor(Math::Color color);
};
static_assert(sizeof(Material) == Material::SIZE);

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
    Hidden      = 1 << 3,
    Light       = 1 << 4,
    Player      = 1 << 5,
    Destroyed   = 1 << 6,
    Lava        = 1 << 7,
    DeathZone   = 1 << 8,
    Checkpoint  = 1 << 9,
    Exit        = 1 << 10
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
    const Shader* shader = nullptr;
    Shape shape = Shape::Rectangle;
    GravityZone gravityZone{};
};

class Scene
{
public:
    std::vector<std::unique_ptr<Entity>> entities;

    struct Properties
    {
        enum class Flags
        {
            LockCameraY  = 1 << 0,
            LockCameraRotation = 1 << 1
        };

        Math::Color backgroundColor;
        uint32_t flags = 0;
    };

    Properties properties;

    Scene() = default;

    Entity* CreateEntity();
    void DestroyEntity(Entity* entity);

    void EndFrame();

    void Clear();

    std::string Serialize() const;
    void Deserialize(const std::string& data);
};
