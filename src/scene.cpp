#include "scene.hpp"

int Scene::CreateEntity()
{
    static uint16_t nextId = 0;
    Entity entity {
        .id = nextId
    };
    entities.push_back(entity);
    return entities.size() - 1;
}
