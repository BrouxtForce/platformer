#include "scene.hpp"

#include <cassert>

Entity* Scene::CreateEntity()
{
    static uint16_t nextId = 1;
    Entity entity {
        .id = nextId++
    };
    assert(!m_EntityMap.contains(entity.id));
    return &m_EntityMap.insert({ entity.id, entity }).first->second;
}
