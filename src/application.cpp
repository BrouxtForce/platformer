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
    Entity& entity = m_Scene.entities[0];
    entity.material.color = Math::Color(0, 0, 1, 1);
    entity.transform.scale = Math::float2(0.1, 0.1);

    Math::float2 movement = 0.0f;
    if (IsKeyDown(SDL_SCANCODE_W)) movement.y += 1.0f;
    if (IsKeyDown(SDL_SCANCODE_A)) movement.x -= 1.0f;
    if (IsKeyDown(SDL_SCANCODE_S)) movement.y -= 1.0f;
    if (IsKeyDown(SDL_SCANCODE_D)) movement.x += 1.0f;
    if (movement.x != 0.0f || movement.y != 0.0f)
    {
        movement = Math::Normalize(movement);
    }

    constexpr float speed = 0.02f;
    entity.transform.position += movement * speed;

    m_Camera.aspect = (float)m_Renderer.GetWidth() / (float)m_Renderer.GetHeight();
    return m_Renderer.Render(m_Scene, m_Camera);
}

void Application::Exit()
{
    SDL_DestroyWindow(m_Window);
}

void Application::OnKeyDown(const SDL_KeyboardEvent& event)
{
    assert(event.scancode >= 0 && event.scancode <= m_KeysDown.size());
    m_KeysDown[event.scancode] = 1;
}

void Application::OnKeyUp(const SDL_KeyboardEvent& event)
{
    assert(event.scancode >= 0 && event.scancode <= m_KeysDown.size());
    m_KeysDown[event.scancode] = 0;
}

bool Application::IsKeyDown(SDL_Scancode scancode)
{
    assert(scancode >= 0 && scancode <= m_KeysDown.size());
    return m_KeysDown[scancode];
}
