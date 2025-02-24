#include "scene.hpp"

#include <SDL3/SDL.h>

#include <cassert>

void Material::WriteColor(Math::Color color)
{
    // By convention, color is usually the first property in the Material struct in the shader
    data[0] = std::bit_cast<uint32_t>(color.r);
    data[1] = std::bit_cast<uint32_t>(color.g);
    data[2] = std::bit_cast<uint32_t>(color.b);
}

namespace Serialization
{
    String& operator<<(String& stream, const Math::float2& vector)
    {
        stream << vector.x << ' ' << vector.y;
        return stream;
    }

    String& operator<<(String& stream, const Math::Color& color)
    {
        stream << color.r << ' ' << color.g << ' ' << color.b << ' ' << color.a;
        return stream;
    }

    String& operator<<(String& stream, const Transform& transform)
    {
        stream << transform.position << ' ' << transform.scale << ' ' << transform.rotation;
        return stream;
    }

    String& operator<<(String& stream, const Material& material)
    {
        bool first = true;
        for (uint32_t value : material.data)
        {
            if (!first)
            {
                stream << ' ';
            }
            first = false;
            stream << value;
        }

        return stream;
    }

    String& operator<<(String& stream, const Shape& shape)
    {
        stream << (uint32_t)shape;
        return stream;
    }

    String& operator<<(String& stream, const GravityZone& gravityZone)
    {
        stream << gravityZone.minAngle << ' ' << gravityZone.maxAngle;
        return stream;
    }

    String& operator<<(String& stream, const Entity& entity)
    {
        stream << entity.flags << ' ';
        stream << entity.zIndex << ' ';
        stream << entity.transform << ' ';
        stream << entity.material << ' ';
        stream << entity.shape << ' ';
        stream << entity.gravityZone;
        return stream;
    }

    String& operator<<(String& stream, const Scene::Properties& properties)
    {
        stream << properties.backgroundColor << ' ' << properties.flags;
        return stream;
    }
}

namespace Deserialization
{
    std::istream& operator>>(std::istream& stream, Math::float2& vector)
    {
        stream >> vector.x >> vector.y;
        return stream;
    }

    std::istream& operator>>(std::istream& stream, Math::Color& color)
    {
        stream >> color.r >> color.g >> color.b >> color.a;
        return stream;
    }

    std::istream& operator>>(std::istream& stream, Transform& transform)
    {
        stream >> transform.position >> transform.scale >> transform.rotation;
        return stream;
    }

    std::istream& operator>>(std::istream& stream, Material& material)
    {
        for (uint32_t& value : material.data)
        {
            stream >> value;
        }

        return stream;
    }

    std::istream& operator>>(std::istream& stream, Shape& shape)
    {
        uint32_t out = 0;
        stream >> out;
        shape = (Shape)out;
        return stream;
    }

    std::istream& operator>>(std::istream& stream, GravityZone& gravityZone)
    {
        stream >> gravityZone.minAngle >> gravityZone.maxAngle;
        return stream;
    }

    std::istream& operator>>(std::istream& stream, Entity& entity)
    {
        stream >> entity.flags;
        stream >> entity.zIndex;
        stream >> entity.transform;
        stream >> entity.material;
        stream >> entity.shape;
        stream >> entity.gravityZone;
        return stream;
    }

    std::istream& operator>>(std::istream& stream, Scene::Properties& properties)
    {
        stream >> properties.backgroundColor;
        stream >> properties.flags;
        return stream;
    }
}

void Scene::Init(MemoryArena* arena)
{
    entities.arena = arena;
}

Entity* Scene::CreateEntity()
{
    Entity* entity = entities.Push(Entity{});
    entity->id = nextId++;
    entity->zIndex = 100;

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
    using namespace Serialization;

    String out;
    out.arena = arena;

    out << properties << '\n';
    bool first = true;
    for (const Entity& entity : entities)
    {
        if ((entity.flags & (uint16_t)EntityFlags::Player) != 0)
        {
            continue;
        }
        if (!first)
        {
            out << '\n';
        }
        first = false;
        out << entity;
    }
    return out;
}

void Scene::Deserialize(StringView data)
{
    using namespace Deserialization;

    // TODO: Remove usage of std::string
    std::stringstream ss((std::string)std::string_view(data.data, data.size));
    ss >> properties;
    while (!ss.eof())
    {
        Entity* entity = CreateEntity();
        ss >> *entity;
    }
}
