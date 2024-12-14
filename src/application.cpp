#include "application.hpp"

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "physics.hpp"
#include "log.hpp"

bool Application::Init()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return false;
    }

    m_Window = SDL_CreateWindow("", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
    if (m_Window == nullptr)
    {
        Log::Error("Failed to create window");
        return false;
    }
    if (!m_Renderer.Init(m_Window))
    {
        Log::Error("Failed to initialize renderer");
        return false;
    }

    m_Scene.Load(Scene::City);

    Entity* playerEntity = m_Scene.CreateEntity();
    playerEntity->material.color = Math::Color(0.5, 0.5, 0.5);
    playerEntity->transform.position = Math::float2(-0.75f, 0.1001f);
    playerEntity->transform.scale = Math::float2(0.1);
    playerEntity->shape = Shape::Ellipse;
    m_Player = Player(playerEntity);

    return true;
}

bool Application::Loop(float deltaTime)
{
    Math::float2 input = 0.0f;
    if (IsKeyDown(SDL_SCANCODE_W)) input.y += 1.0f;
    if (IsKeyDown(SDL_SCANCODE_A)) input.x -= 1.0f;
    if (IsKeyDown(SDL_SCANCODE_S)) input.y -= 1.0f;
    if (IsKeyDown(SDL_SCANCODE_D)) input.x += 1.0f;
    if (input.x != 0.0f || input.y != 0.0f)
    {
        input = Math::Normalize(input);
    }
    m_Player.Move(m_Scene, input);
    m_Camera.FollowTransform(m_Player.GetTransform(), deltaTime, 0.15f);

    m_Renderer.Resize();
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
