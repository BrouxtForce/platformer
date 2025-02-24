#pragma once

#include <memory>

#include <SDL3/SDL.h>
#include <webgpu/webgpu.hpp>

#include "renderer.hpp"
#include "player.hpp"
#include "menu.hpp"
#include "input.hpp"
#include "memory-arena.hpp"

// Memory allocated with this arena will persist for the entire duration of the program
extern MemoryArena GlobalArena;

// Memory allocated in this arena will be cleared at the beginning of every frame
extern MemoryArena TransientArena;

enum class GameState
{
    MainMenu_MainMenu = 1 << 0,
    MainMenu_Controls = 1 << 1,

    Game   = 1 << 2,
    Editor = 1 << 3,

    FinishingLevel = 1 << 4,

    MainMenu = MainMenu_MainMenu | MainMenu_Controls
};

class Application
{
public:
    Application() = default;

    bool Init(GameState startGameState);
    bool Loop(float deltaTime);
    void Exit();

    void LoadScene(StringView sceneName);

    void OnEvent(const SDL_Event& event);

private:
    static constexpr int WINDOW_WIDTH = 640;
    static constexpr int WINDOW_HEIGHT = 480;

    static constexpr StringView firstSceneFilepath = "assets/scenes/castle.txt";

    SDL_Window* m_Window = nullptr;
    Renderer m_Renderer;
    Input m_Input;

    std::array<int, 2> m_ActiveControlRebind = { -1, -1 };

    GameState m_GameState = GameState::MainMenu_MainMenu;

    static constexpr uint16_t s_LevelFinishFrames = 420;
    uint16_t m_LevelFinishCounter = 0;

    Menu m_Menu;

    Scene m_Scene;
    Camera m_Camera;

    Player m_Player;

    bool LoopEditor(float deltaTime);
    bool LoopMainMenu(float deltaTime);
    bool LoopGame(float deltaTime);
};
