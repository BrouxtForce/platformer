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

    m_PlayerEntity = m_Scene.CreateEntity();
    m_PlayerEntity->material.color = Math::Color(0, 0, 1, 1);
    m_PlayerEntity->transform.scale = Math::float2(0.1, 0.15);
    m_PlayerEntity->shape = Shape::Ellipse;

    Entity* colliderEntityA = m_Scene.CreateEntity();
    colliderEntityA->transform = Transform(
        Math::float2(-0.5f, -0.5f),
        Math::float2(0.2f, 0.2f)
    );
    colliderEntityA->material.color = Math::Color(0, 0, 0, 1);
    colliderEntityA->flags |= (uint16_t)EntityFlags::Collider;

    Entity* colliderEntityB = m_Scene.CreateEntity();
    colliderEntityB->transform = Transform(
        Math::float2(0.5f, 0.4f),
        Math::float2(0.4f, 0.1f)
    );
    colliderEntityB->material.color = Math::Color(0, 0, 0, 1);
    colliderEntityB->flags |= (uint16_t)EntityFlags::Collider;

    return true;
}

bool Application::Loop()
{
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
    m_PlayerEntity->transform.position += Physics::CollideAndSlide(m_Scene, m_PlayerEntity->transform, movement * speed);

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
