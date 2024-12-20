#include "application.hpp"

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <imgui.h>
#include <imgui_impl_sdl3.h>

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
    switch (m_GameState)
    {
        case GameState::MainMenu:
            return LoopMainMenu(deltaTime);
        case GameState::Game:
            return LoopGame(deltaTime);
        default:
            Log::Error("Invalid game state: " + std::to_string((int)m_GameState));
            return false;
    }
    return true;
}

bool Application::LoopMainMenu(float /* deltaTime */)
{
    m_Renderer.NewFrame();

    Math::float2 mousePosition = 0.0f;
    SDL_MouseButtonFlags flags = SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

    mousePosition /= Math::float2(m_Renderer.GetWidth(), m_Renderer.GetHeight());
    mousePosition = mousePosition * 2.0f - 1.0f;
    mousePosition *= { (float)m_Renderer.GetWidth() / m_Renderer.GetHeight(), -1.0f };

    m_Menu.Begin(mousePosition, (bool)(flags & SDL_BUTTON_LMASK));
    m_Menu.SetBackgroundColor({ 0.2, 0.2, 0.2 });
    m_Menu.SetFillColor({ 0.5, 0.5, 0.5 });
    if (m_Menu.Button("Start Game", { 0.0f, 0.5f }, { 0.5f, 0.07f }, { 0.0f, 0.05f }))
    {
        m_GameState = GameState::Game;
    }

    ImGui::Text("Mouse position: (%f, %f)", mousePosition.x, mousePosition.y);

    m_Renderer.Resize();

    Camera camera { .aspect = (float)m_Renderer.GetWidth() / m_Renderer.GetHeight() };
    return m_Renderer.Render(m_Menu.scene, camera);
}

bool Application::LoopGame(float deltaTime)
{
    static double frameTime = 0.0;
    uint64_t frameStart = SDL_GetTicksNS();

    m_Renderer.NewFrame();

    Math::float2 input = 0.0f;
    if (IsKeyDown(SDL_SCANCODE_W)) input.y += 1.0f;
    if (IsKeyDown(SDL_SCANCODE_A)) input.x -= 1.0f;
    if (IsKeyDown(SDL_SCANCODE_S)) input.y -= 1.0f;
    if (IsKeyDown(SDL_SCANCODE_D)) input.x += 1.0f;
    m_Player.Move(m_Scene, input, IsKeyPressed(SDL_SCANCODE_W) || IsKeyPressed(SDL_SCANCODE_SPACE));
    m_Camera.FollowTransform(m_Player.GetTransform(), deltaTime, 0.15f);

    m_KeysPressed.reset();

    ImGui::Text("CPU: %fms", frameTime);
    ImGui::Text("Delta time: %fms", deltaTime * 1000.0f);
    ImGui::Text("Camera position: (%f, %f)", m_Camera.transform.position.x, m_Camera.transform.position.y);
    ImGui::Text("Player position: (%f, %f)", m_Player.GetTransform().position.x, m_Player.GetTransform().position.y);
    ImGui::Text("Player velocity: (%f, %f)", m_Player.velocity.x, m_Player.velocity.y);
    ImGui::Text("Player speed: %f", Math::Length(m_Player.velocity));
    ImGui::DragFloat("Player acceleration", &m_Player.acceleration, 0.001f);
    ImGui::DragFloat("Player jump acceleration", &m_Player.jumpAcceleration, 0.001f);
    ImGui::DragFloat("Player gravity acceleration", &m_Player.gravityAcceleration, 0.001f);
    ImGui::DragFloat("Player drag", &m_Player.drag, 0.001f);
    ImGui::Text("Gravity direction: (%f, %f)", m_Player.gravityDirection.x, m_Player.gravityDirection.y);

    if (ImGui::TreeNode("Debug Textures"))
    {
        m_Renderer.ImGuiDebugTextures();
        ImGui::TreePop();
    }

    m_Renderer.Resize();
    m_Camera.aspect = (float)m_Renderer.GetWidth() / (float)m_Renderer.GetHeight();

    frameTime = (double)(SDL_GetTicksNS() - frameStart) / 1'000'000.0;
    return m_Renderer.Render(m_Scene, m_Camera);
}

void Application::Exit()
{
    SDL_DestroyWindow(m_Window);
}

void Application::OnEvent(const SDL_Event& event)
{
    ImGui_ImplSDL3_ProcessEvent(&event);
}

void Application::OnKeyDown(const SDL_KeyboardEvent& event)
{
    assert(event.scancode >= 0 && event.scancode <= m_KeysDown.size());
    if (!event.repeat)
    {
        m_KeysDown[event.scancode] = 1;
        m_KeysPressed[event.scancode] = 1;
    }
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

bool Application::IsKeyPressed(SDL_Scancode scancode)
{
    assert(scancode >= 0 && scancode < m_KeysPressed.size());
    return m_KeysPressed[scancode];
}
