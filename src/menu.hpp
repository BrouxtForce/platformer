#pragma once

#include "math.hpp"
#include "scene.hpp"
#include "data-structures.hpp"

class Menu
{
public:
    Scene scene;

    void Init(MemoryArena* arena);

    void Begin(Math::float2 mousePosition, bool mousePressed);

    void SetBackgroundColor(Math::Color color);
    void SetFillColor(Math::Color color);

    void Text(StringView text, Math::float2 center, float scale);
    bool Button(StringView text, Math::float2 center, Math::float2 extent, Math::float2 padding = 0.0f);

private:
    Math::float2 m_MousePosition{};
    bool m_MousePressed = false;

    Math::Color m_BackgroundColor{};
    Math::Color m_FillColor{};

    Entity* GetNextEntity();
};
