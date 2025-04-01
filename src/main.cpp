#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "application.hpp"
#include "log.hpp"

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

static Application application;

SDL_AppResult SDL_AppInit(void** /* state */, int argc, char** argv)
{
    GameState startGameState = GameState::MainMenu_MainMenu;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-v") == 0)
        {
            Log::SetLogLevel(Log::LogLevel::Debug);
        }
        else if (strcmp(argv[i], "-q") == 0)
        {
            Log::SetLogLevel(Log::LogLevel::None);
        }
        else if (strcmp(argv[i], "-e") == 0)
        {
            startGameState = GameState::Editor;
        }
    }
    if (application.Init(startGameState))
    {
        return SDL_APP_CONTINUE;
    }
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppIterate(void* /* state */)
{
#if __EMSCRIPTEN__
    double thisFrameMs = emscripten_get_now();
    static double lastFrameMs = thisFrameMs;
    double deltaTime = (thisFrameMs - lastFrameMs) / 1'000.0f;
    lastFrameMs = thisFrameMs;
#else
    uint64_t thisFrameTicks = SDL_GetTicksNS();
    static uint64_t lastFrameTicks = thisFrameTicks;
    double deltaTime = (double)(thisFrameTicks - lastFrameTicks) / 1'000'000'000.0;
    lastFrameTicks = thisFrameTicks;
#endif

    if (application.Loop(deltaTime))
    {
        return SDL_APP_CONTINUE;
    }
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void* /* state */, SDL_Event* event)
{
    application.OnEvent(*event);
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* /* state */, SDL_AppResult /* result */)
{
    application.Exit();
}
