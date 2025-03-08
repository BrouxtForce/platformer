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

    void SetButtonMaterial(StringView materialName);
    void SetTextMaterial(StringView materialName);

    void Text(StringView text, Math::float2 center, float scale);
    bool Button(StringView text, Math::float2 center, Math::float2 extent, Math::float2 padding = 0.0f);

private:
    Math::float2 m_MousePosition{};
    bool m_MousePressed = false;

    Material* m_ButtonMaterial = nullptr;
    Material* m_TextMaterial = nullptr;
};
