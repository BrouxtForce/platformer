#include "menu.hpp"
#include "shader-library.hpp"
#include "material.hpp"

void Menu::Init(MemoryArena* arena)
{
    scene.Init(arena);
}

void Menu::Begin(Math::float2 mousePosition, bool mousePressed)
{
    m_MousePosition = mousePosition;
    m_MousePressed = mousePressed;

    for (Entity& entity : scene.entities)
    {
        entity.flags |= (uint16_t)EntityFlags::Hidden;
    }
    scene.Clear();
}

void Menu::SetButtonMaterial(StringView materialName)
{
    m_ButtonMaterial = MaterialManager::GetMaterial(materialName);
}

void Menu::SetTextMaterial(StringView materialName)
{
    m_TextMaterial = MaterialManager::GetMaterial(materialName);
}

void Menu::Text(StringView text, Math::float2 center, float scale)
{
    assert(m_TextMaterial != nullptr);

    Entity* entity = scene.CreateEntity();
    entity->name += text;
    entity->flags = (uint16_t)EntityFlags::Text;
    entity->transform.position = center;
    entity->transform.scale = scale;
    entity->material = m_TextMaterial;
}

bool Menu::Button(StringView text, Math::float2 center, Math::float2 extent, Math::float2 padding)
{
    assert(m_ButtonMaterial != nullptr && m_TextMaterial != nullptr);

    Entity* entity = scene.CreateEntity();
    entity->flags = (uint16_t)EntityFlags::None;
    entity->transform.position = center;
    entity->transform.scale = extent + padding;
    entity->shape = Shape::Rectangle;
    entity->material = m_ButtonMaterial;

    entity = scene.CreateEntity();
    entity->name += text;
    entity->flags = (uint16_t)EntityFlags::Text;
    entity->transform.position = center;
    entity->transform.scale = extent.y;
    entity->material = m_TextMaterial;

    if (!m_MousePressed)
    {
        return false;
    }

    return m_MousePosition.x > center.x - extent.x - padding.x &&
           m_MousePosition.y > center.y - extent.y - padding.y &&
           m_MousePosition.x < center.x + extent.x + padding.x &&
           m_MousePosition.y < center.y + extent.y + padding.y;
}
