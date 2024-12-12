#include "scene.hpp"

#include <SDL3/SDL.h>

#include <cassert>

Entity* Scene::CreateEntity()
{
    static uint16_t nextId = 1;
    Entity entity {
        .id = nextId++,
        .zIndex = 100
    };
    assert(!m_EntityMap.contains(entity.id));
    return &m_EntityMap.insert({ entity.id, entity }).first->second;
}

struct Collider
{
    Transform transform;
    Shape shape;
};

struct SceneDescription
{
    std::string name;
    Math::Color backgroundColor;
    std::vector<Collider> colliders;
    uint64_t seed;
};

static std::array<SceneDescription, 1> s_SceneDescriptions = {
    SceneDescription {
        .name = "City",
        .backgroundColor = Math::Color(0.1, 0.1, 0.1),
        .colliders = {
            { Transform(Math::float2(-1.0f, -0.25f), Math::float2(1.0f, 0.25f)), Shape::Rectangle },
            { Transform(Math::float2(0.0f, -0.5f),    Math::float2(0.5f)       ), Shape::Ellipse },
            { Transform(Math::float2(0.25f, -1.0f),  Math::float2(0.25f, 0.5f)), Shape::Rectangle },
        },
        .seed = 892542184
    }
};

void Scene::Load(int index)
{
    assert(index >= 0 && index < (int)s_SceneDescriptions.size());
    const SceneDescription& description = s_SceneDescriptions[index];

    SDL_srand(description.seed);

    properties.backgroundColor = description.backgroundColor;

    for (const Collider& collider : description.colliders)
    {
        Entity* entity = CreateEntity();
        entity->transform = collider.transform;
        entity->flags = (uint16_t)EntityFlags::Collider;
        entity->material.color = Math::Color(0.2, 0.2, 0.2);
        entity->shape = collider.shape;

        entity->zIndex = 2;
    }

    constexpr int NUM_BUILDINGS = 15;
    constexpr float BUILDING_RANGE = 2;
    constexpr float MIN_DELTA = 0.05;
    constexpr float MAX_DELTA = 0.3;
    constexpr float MIN_HEIGHT = 0.1f;
    constexpr float MAX_HEIGHT = 0.9f;

    float prevBuildingHeight = (float)(MIN_HEIGHT + MAX_HEIGHT) / 2.0f;
    for (int i = 0; i < NUM_BUILDINGS; i++)
    {
        Entity* entity = CreateEntity();

        constexpr float BUILDING_WIDTH = BUILDING_RANGE * 2.0f / NUM_BUILDINGS;
        entity->transform.scale.x = BUILDING_WIDTH / 2.0f;
        do
        {
            float rand = SDL_randf() * 2.0f - 1.0f;
            entity->transform.scale.y = prevBuildingHeight + rand * (MAX_DELTA - MIN_DELTA) + std::copysign(MIN_DELTA, rand);
        }
        while (entity->transform.scale.y < MIN_HEIGHT || entity->transform.scale.y > MAX_HEIGHT);
        prevBuildingHeight = entity->transform.scale.y;
        entity->transform.position.x = BUILDING_WIDTH * ((float)i - (float)NUM_BUILDINGS / 2.0f);
        entity->transform.position.y = -1.0f + entity->transform.scale.y;

        entity->material.color = Math::Color(0.13, 0.13, 0.13);

        entity->zIndex = 1;
    }
}
