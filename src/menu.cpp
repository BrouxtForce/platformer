#include "menu.hpp"

void Menu::Begin(Math::float2 mousePosition, bool mousePressed)
{
    m_MousePosition = mousePosition;
    m_MousePressed = mousePressed;

    for (const std::unique_ptr<Entity>& entity : scene.entities)
    {
        entity->flags |= (uint16_t)EntityFlags::Hidden;
    }
    m_EntityIndex = 0;
}

void Menu::SetBackgroundColor(Math::Color color)
{
    m_BackgroundColor = color;
}

void Menu::SetFillColor(Math::Color color)
{
    m_FillColor = color;
}

void Menu::Text(const std::string& text, Math::float2 center, float scale)
{
    Entity* entity = GetNextEntity();
    entity->name = text;
    entity->flags = (uint16_t)EntityFlags::Text;
    entity->transform.position = center;
    entity->transform.scale = scale;
    entity->material.WriteColor(m_FillColor);
}

bool Menu::Button(const std::string& text, Math::float2 center, Math::float2 extent, Math::float2 padding)
{
    Entity* entity = GetNextEntity();
    entity->flags = (uint16_t)EntityFlags::None;
    entity->transform.position = center;
    entity->transform.scale = extent + padding;
    entity->shape = Shape::Rectangle;
    entity->material.WriteColor(m_BackgroundColor);

    entity = GetNextEntity();
    entity->name = text;
    entity->flags = (uint16_t)EntityFlags::Text;
    entity->transform.position = center;
    entity->transform.scale = extent.y;
    entity->material.WriteColor(m_FillColor);

    if (!m_MousePressed)
    {
        return false;
    }

    return m_MousePosition.x > center.x - extent.x - padding.x &&
           m_MousePosition.y > center.y - extent.y - padding.y &&
           m_MousePosition.x < center.x + extent.x + padding.x &&
           m_MousePosition.y < center.y + extent.y + padding.y;
}

Entity* Menu::GetNextEntity()
{
    Entity* entity = nullptr;
    if (m_EntityIndex >= (int)scene.entities.size())
    {
        entity = scene.CreateEntity();
    }
    else
    {
        entity = scene.entities[m_EntityIndex].get();
    }
    m_EntityIndex++;
    return entity;
}
