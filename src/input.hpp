#pragma once

#include <bitset>

#include <SDL3/SDL.h>

#include "math.hpp"

enum class Key
{
    Up, Left, Down, Right,
    Jump,
    Boost,

    Count
};

constexpr static inline std::array<std::string_view, (int)Key::Count> KeyNames
{
    "Up", "Left", "Down", "Right", "Jump", "Boost"
};

class Input
{
public:
    std::array<std::array<SDL_Scancode, 3>, (int)Key::Count> controls{};

    Input();

    Math::float2 Joystick() const;

    bool IsKeyDown(Key key) const;
    bool IsKeyPressed(Key key) const;

    SDL_Scancode GetFirstKeyPressed() const;

    Math::float2 GetMousePosition() const;

    bool IsMouseDown() const;
    bool IsMousePressed() const;

    void OnEvent(const SDL_Event& event);

    void EndFrame();

private:
    std::bitset<SDL_SCANCODE_COUNT> m_KeysDown;
    std::bitset<SDL_SCANCODE_COUNT> m_KeysPressed;

    SDL_Scancode m_FirstKeyPressed = SDL_SCANCODE_UNKNOWN;

    Math::float2 m_MousePosition = 0.0f;
    uint8_t m_MouseDownState = 0;
    uint8_t m_MousePressedState = 0;

    bool IsKeyDown(SDL_Scancode scancode) const;
    bool IsKeyPressed(SDL_Scancode scancode) const;

    friend class Application;
};
