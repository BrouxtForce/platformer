#include "scene.hpp"

#include <SDL3/SDL.h>

#include <cassert>

#include "application.hpp"
#include "config.hpp"
#include "material.hpp"

void Scene::Init(MemoryArena* arena)
{
    entities.arena = arena;
}

Entity* Scene::CreateEntity()
{
    Entity* entity = entities.Push(Entity{});
    entity->id = nextId++;
    entity->zIndex = 100;
    entity->material = MaterialManager::GetDefaultMaterial();

    // TODO: When entities are destroyed, memory from the named string is not recycled
    entity->name.arena = &GlobalArena;
    // TODO: Is this a good default size?
    entity->name.Reserve(16);

    return entity;
}

void Scene::DestroyEntity(Entity* entity)
{
    assert(entity != nullptr);
    assert((entity->flags & (uint16_t)EntityFlags::Destroyed) == 0);
    // entity->flags |= (uint16_t)EntityFlags::Destroyed;
    entities.Erase(entity);
}

void Scene::EndFrame()
{
    // TODO: Remove
}

void Scene::Clear()
{
    entities.Clear();
    nextId = 0;
}

// TODO: Serialize and deserialize Entity::name
String Scene::Serialize(MemoryArena* arena) const
{
    String out;
    out.arena = arena;

    Math::Color color = properties.backgroundColor;
    out << "background_color = [" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << "]\n";
    out << "flags = " << properties.flags << "\n\n";

    // TODO: Automatic unique entity naming scheme
    for (const Entity& entity : entities)
    {
        if ((entity.flags & (uint16_t)EntityFlags::Player) != 0)
        {
            continue;
        }
        out << '[' << entity.name << "]\n";
        out << "flags = " << entity.flags << '\n';
        out << "z_index = " << entity.zIndex << '\n';
        out << "position = [" << entity.transform.position.x << ", " << entity.transform.position.y << "]\n";
        if (entity.transform.rotation != 0.0f)
        {
            out << "rotation = " << entity.transform.rotation << '\n';
        }
        out << "scale = [" << entity.transform.scale.x << ", " << entity.transform.scale.y << "]\n";
        if (entity.material != nullptr)
        {
            out << "material = \"" << entity.material->name << "\"\n";
        }
        out << "shape = " << (int)entity.shape << '\n';
        if (entity.flags & (uint16_t)EntityFlags::GravityZone)
        {
            out << "gravity_zone = [" << entity.gravityZone.minAngle << ", " << entity.gravityZone.maxAngle << "]\n";
        }
        // TODO: Don't input an extra newline for the final line
        out << '\n';
    }
    return out;
}

void Scene::Deserialize(StringView data)
{
    Config::SetMemoryArena(&TransientArena);
    Config::LoadRaw(data);

    properties.backgroundColor = Config::Get<Math::float4>("background_color", {});
    properties.flags = Config::Get<int32_t>("flags", 0);

    Span<StringView> tables = Config::GetTables(&TransientArena);
    for (StringView table : tables)
    {
        Config::PushTable(table);

        Entity* entity = CreateEntity();
        entity->name = String::Copy(table, &GlobalArena);

        entity->flags = Config::Get<int32_t>("flags", 0);
        entity->zIndex = Config::Get<int32_t>("z_index", 0);

        entity->transform.position = Config::Get<Math::float2>("position", 0.0f);
        Config::SuppressWarnings(true);
        entity->transform.rotation = Config::Get<float>("rotation", 0.0f);
        Config::SuppressWarnings(false);
        entity->transform.scale = Config::Get<Math::float2>("scale", 1.0f);

        entity->material = MaterialManager::GetMaterial(Config::Get<StringView>("material", ""));
        entity->shape = (Shape)Config::Get<int32_t>("shape", (int32_t)Shape::Rectangle);

        Config::SuppressWarnings(true);
        Math::float2 gravityZone = Config::Get<Math::float2>("gravity_zone", 0.0f);
        entity->gravityZone.minAngle = gravityZone.x;
        entity->gravityZone.maxAngle = gravityZone.x;
        Config::SuppressWarnings(false);

        Config::PopTable();
    }

    Config::PopTable();
}
