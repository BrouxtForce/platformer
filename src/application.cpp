#include "application.hpp"

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

bool Application::Init()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return false;
    }

    m_Window = SDL_CreateWindow("", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (m_Window == nullptr)
    {
        return false;
    }
    if (!m_Renderer.Init(m_Window))
    {
        return false;
    }

    m_Scene.CreateEntity();

    return true;
}

bool Application::Loop()
{
    m_Scene.entities[0].material.color = Math::Color(SDL_randf(), SDL_randf(), SDL_randf(), 1);
    return m_Renderer.Render(m_Scene);
}

void Application::Exit()
{
    SDL_DestroyWindow(m_Window);
}
