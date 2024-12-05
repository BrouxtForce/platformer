#pragma once

#include <memory>

#include <SDL3/SDL.h>
#include <webgpu/webgpu.hpp>

#include "renderer.hpp"

class Application
{
public:
    Application() = default;

    bool Init();
    bool Loop();
    void Exit();

private:
    static constexpr int WINDOW_WIDTH = 640;
    static constexpr int WINDOW_HEIGHT = 480;

    SDL_Window* m_Window = nullptr;
    Renderer m_Renderer;

    Scene m_Scene;
};
