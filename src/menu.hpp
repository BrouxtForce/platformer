#pragma once

#include <string>

#include "math.hpp"
#include "scene.hpp"

class Menu
{
public:
    Scene scene;

    Menu() = default;

    void Begin(Math::float2 mousePosition, bool mousePressed);

    void SetBackgroundColor(Math::Color color);
    void SetFillColor(Math::Color color);

    void Text(const std::string& text, Math::float2 center, Math::float2 extent);
    bool Button(const std::string& text, Math::float2 center, Math::float2 extent, Math::float2 padding = 0.0f);

private:
    int m_EntityIndex = 0;

    Math::float2 m_MousePosition{};
    bool m_MousePressed = false;

    Math::Color m_BackgroundColor;
    Math::Color m_FillColor;

    Entity* GetNextEntity();
};
