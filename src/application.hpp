#pragma once

#include <memory>

#include <SDL3/SDL.h>
#include <webgpu/webgpu.hpp>

#include "renderer.hpp"
#include "player.hpp"
#include "menu.hpp"
#include "input.hpp"

enum class GameState
{
    MainMenu_MainMenu = 1 << 0,
    MainMenu_Controls = 1 << 1,

    Game = 1 << 2,

    MainMenu = MainMenu_MainMenu | MainMenu_Controls
};

class Application
{
public:
    Application() = default;

    bool Init();
    bool Loop(float deltaTime);
    void Exit();

    void OnEvent(const SDL_Event& event);

private:
    static constexpr int WINDOW_WIDTH = 640;
    static constexpr int WINDOW_HEIGHT = 480;

    SDL_Window* m_Window = nullptr;
    Renderer m_Renderer;
    Input m_Input;

    std::array<int, 2> m_ActiveControlRebind = { -1, -1 };

    GameState m_GameState = GameState::MainMenu_MainMenu;

    Menu m_Menu;

    Scene m_Scene;
    Camera m_Camera;

    Player m_Player;

    bool LoopMainMenu(float deltaTime);
    bool LoopGame(float deltaTime);
};
