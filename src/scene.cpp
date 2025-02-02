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
    std::ostream& operator<<(std::ostream& stream, const Math::float2& vector)
    {
        stream << vector.x << ' ' << vector.y;
        return stream;
    }

    std::ostream& operator<<(std::ostream& stream, const Math::Color& color)
    {
        stream << color.r << ' ' << color.g << ' ' << color.b << ' ' << color.a;
        return stream;
    }

    std::ostream& operator<<(std::ostream& stream, const Transform& transform)
    {
        stream << transform.position << ' ' << transform.scale << ' ' << transform.rotation;
        return stream;
    }

    std::ostream& operator<<(std::ostream& stream, const Material& material)
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

    std::ostream& operator<<(std::ostream& stream, const Shape& shape)
    {
        stream << (uint32_t)shape;
        return stream;
    }

    std::ostream& operator<<(std::ostream& stream, const GravityZone& gravityZone)
    {
        std::streamsize prevPrecision = stream.precision(8);
        stream << gravityZone.minAngle << ' ' << gravityZone.maxAngle;
        stream.precision(prevPrecision);
        return stream;
    }

    std::ostream& operator<<(std::ostream& stream, const Entity& entity)
    {
        stream << entity.flags << ' ';
        stream << entity.zIndex << ' ';
        stream << entity.transform << ' ';
        stream << entity.material << ' ';
        stream << entity.shape << ' ';
        stream << entity.gravityZone;
        return stream;
    }

    std::ostream& operator<<(std::ostream& stream, const Scene::Properties& properties)
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

Entity* Scene::CreateEntity()
{
    static uint16_t nextId = 1;

    entities.push_back(std::make_unique<Entity>());
    Entity* entity = entities.back().get();
    entity->id = nextId++;
    entity->zIndex = 100;

    return entity;
}

void Scene::DestroyEntity(Entity* entity)
{
    assert(entity != nullptr);
    assert((entity->flags & (uint16_t)EntityFlags::Destroyed) == 0);
    entity->flags |= (uint16_t)EntityFlags::Destroyed;
}

void Scene::EndFrame()
{
    int lastEntityIndex = entities.size() - 1;
    for (int i = (int)entities.size() - 1; i >= 0; i--)
    {
        if ((entities[i]->flags & (uint16_t)EntityFlags::Destroyed) == 0)
        {
            continue;
        }
        entities[i] = std::move(entities[lastEntityIndex]);
        lastEntityIndex--;
    }
    entities.resize(lastEntityIndex + 1);
}

void Scene::Clear()
{
    entities.clear();
}

// TODO: Serialize and deserialize Entity::name
std::string Scene::Serialize() const
{
    using namespace Serialization;

    std::stringstream ss;
    ss << properties << '\n';
    for (int i = 0; i < (int)entities.size(); i++)
    {
        if ((entities[i]->flags & (uint16_t)EntityFlags::Player) != 0)
        {
            continue;
        }
        if (i != 0)
        {
            ss << '\n';
        }
        ss << *entities[i];
    }
    return ss.str();
}

void Scene::Deserialize(const std::string& data)
{
    using namespace Deserialization;

    std::stringstream ss(data);
    ss >> properties;
    while (!ss.eof())
    {
        Entity* entity = CreateEntity();
        ss >> *entity;
    }
}
