#include "application.hpp"

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
    if ((int)m_GameState & (int)GameState::MainMenu)
    {
        return LoopMainMenu(deltaTime);
    }
    if (m_GameState == GameState::Game)
    {
        return LoopGame(deltaTime);
    }
    Log::Error("Invalid game state: " + std::to_string((int)m_GameState));
    return false;
}

bool Application::LoopMainMenu(float /* deltaTime */)
{
    m_Renderer.NewFrame();

    Math::float2 mousePosition = m_Input.GetMousePosition();
    mousePosition /= Math::float2(m_Renderer.GetWidth(), m_Renderer.GetHeight());
    mousePosition = mousePosition * 2.0f - 1.0f;
    mousePosition *= { (float)m_Renderer.GetWidth() / m_Renderer.GetHeight(), -1.0f };

    m_Menu.Begin(mousePosition, m_Input.IsMousePressed());

    m_Menu.SetBackgroundColor({ 0.2, 0.2, 0.2 });
    m_Menu.SetFillColor({ 0.5, 0.5, 0.5 });

    const Math::float2 buttonSize = { 0.5f, 0.07f };
    const Math::float2 buttonPadding = { 0.0f, 0.05f };

    bool shouldExit = false;
    switch (m_GameState)
    {
        case GameState::MainMenu_MainMenu:
            m_Menu.Text("Untitled Platformer", { 0.0f, 0.5f }, 0.1f);
            if (m_Menu.Button("Start Game", { 0.0f, 0.05f }, buttonSize, buttonPadding))
            {
                m_GameState = GameState::Game;
            }
            if (m_Menu.Button("Controls", { 0.0f, -0.25f }, buttonSize, buttonPadding))
            {
                m_GameState = GameState::MainMenu_Controls;
            }
            if (m_Menu.Button("Exit Game", { 0.0f, -0.55f }, buttonSize, buttonPadding))
            {
                shouldExit = true;
            }
            break;
        case GameState::MainMenu_Controls:
            m_Menu.Text("Controls", { 0.0f, 0.75f }, 0.1f);
            m_Menu.Text("Click a on key binding to change it.", { 0.0f, 0.57f }, 0.035f);
            m_Menu.Text("Enter ESCAPE to reset a binding.",     { 0.0f, 0.49f }, 0.035f);
            for (int i = 0; i < (int)m_Input.controls.size(); i++)
            {
                float y = (float)(2 - i) * 0.17f;
                m_Menu.Text((std::string)KeyNames[i], { -0.8f, y }, 0.06f);
                for (int j = 0; j < (int)m_Input.controls[i].size(); j++)
                {
                    bool selected = i == m_ActiveControlRebind[0] && j == m_ActiveControlRebind[1];
                    if (selected && m_Input.GetFirstKeyPressed() != SDL_SCANCODE_UNKNOWN)
                    {
                        if (m_Input.GetFirstKeyPressed() == SDL_SCANCODE_ESCAPE)
                        {
                            m_Input.controls[i][j] = SDL_SCANCODE_UNKNOWN;
                        }
                        else {
                            m_Input.controls[i][j] = m_Input.GetFirstKeyPressed();
                        }
                        selected = false;
                        m_ActiveControlRebind = { -1, -1 };
                    }

                    SDL_Scancode scancode = m_Input.controls[i][j];
                    SDL_Keycode keycode = SDL_GetKeyFromScancode(scancode, 0, true);
                    const char* keyName = nullptr;
                    if (selected)
                    {
                        keyName = "[Press any key]";
                    }
                    else
                    {
                        switch (keycode)
                        {
                            case SDLK_MINUS:        keyName = "Minus";         break;
                            case SDLK_EQUALS:       keyName = "Equals";        break;
                            case SDLK_LEFTBRACKET:  keyName = "Left Bracket";  break;
                            case SDLK_RIGHTBRACKET: keyName = "Right Bracket"; break;
                            case SDLK_BACKSLASH:    keyName = "Backslash";     break;
                            case SDLK_HASH:         keyName = "Hashtag";       break;
                            case SDLK_SEMICOLON:    keyName = "Semicolon";     break;
                            case SDLK_APOSTROPHE:   keyName = "Apostrophe";    break;
                            case SDLK_GRAVE:        keyName = "Backtick";      break;
                            case SDLK_COMMA:        keyName = "Comma";         break;
                            case SDLK_PERIOD:       keyName = "Period";        break;
                            case SDLK_SLASH:        keyName = "Forward Slash"; break;
                            default: keyName = SDL_GetKeyName(keycode);
                        }
                    }

                    float x = (float)j * 0.5f - 0.3f;
                    if (m_Menu.Button(keyName, { x, y }, { 0.23f, 0.03f }, { 0.0f, 0.03f }))
                    {
                        m_ActiveControlRebind = { i, j };
                    }
                }
            }
            if (m_Menu.Button("Back", { 0.0f, -0.7f }, buttonSize, buttonPadding))
            {
                m_GameState = GameState::MainMenu_MainMenu;
                m_ActiveControlRebind = { -1, -1 };
            }
            break;
        default:
            Log::Error("Invalid game state: " + std::to_string((int)m_GameState));
            break;
    }

    m_Input.EndFrame();

    ImGui::Text("Mouse position: (%f, %f)", mousePosition.x, mousePosition.y);

    m_Renderer.Resize();

    Camera camera { .aspect = (float)m_Renderer.GetWidth() / m_Renderer.GetHeight() };
    return m_Renderer.Render(m_Menu.scene, camera) && !shouldExit;
}

bool Application::LoopGame(float deltaTime)
{
    static double frameTime = 0.0;
    uint64_t frameStart = SDL_GetTicksNS();

    m_Renderer.NewFrame();

    m_Player.Move(m_Scene, m_Input.Joystick(), m_Input.IsKeyPressed(Key::Jump));
    m_Camera.FollowTransform(m_Player.GetTransform(), deltaTime, 0.15f);

    m_Input.EndFrame();

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
    m_Input.OnEvent(event);
}
