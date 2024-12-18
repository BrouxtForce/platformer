#pragma once

#include <memory>

#include <SDL3/SDL.h>
#include <webgpu/webgpu.hpp>

#include "renderer.hpp"
#include "player.hpp"

class Application
{
public:
    Application() = default;

    bool Init();
    bool Loop(float deltaTime);
    void Exit();

    void OnEvent(const SDL_Event& event);
    void OnKeyDown(const SDL_KeyboardEvent& event);
    void OnKeyUp(const SDL_KeyboardEvent& event);

    bool IsKeyDown(SDL_Scancode scancode);
    bool IsKeyPressed(SDL_Scancode scancode);

private:
    static constexpr int WINDOW_WIDTH = 640;
    static constexpr int WINDOW_HEIGHT = 480;

    SDL_Window* m_Window = nullptr;
    Renderer m_Renderer;

    Scene m_Scene;
    Camera m_Camera;

    std::bitset<SDL_SCANCODE_COUNT> m_KeysDown;
    std::bitset<SDL_SCANCODE_COUNT> m_KeysPressed;

    Player m_Player;
};
