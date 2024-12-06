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
    static int frame = 0;
    frame++;

    float cos = SDL_cosf((float)frame / 60.0f);
    float sin = SDL_sinf((float)frame / 60.0f);

    Entity& entity = m_Scene.entities[0];
    entity.material.color = Math::Color((cos + 1.0f) / 2.0f, (sin + 1.0f) / 2.0f, 0.0f, 1.0f);
    entity.transform.position = Math::float2(cos, sin) * 0.5f;
    entity.transform.scale = (cos + sin) * 0.25f;

    return m_Renderer.Render(m_Scene);
}

void Application::Exit()
{
    SDL_DestroyWindow(m_Window);
}
