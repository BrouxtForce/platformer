#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "application.hpp"
#include "log.hpp"

SDL_AppResult SDL_AppInit(void** state, int argc, char** argv)
{
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
    }

    Application* application = new Application();
    *state = application;
    if (application->Init())
    {
        return SDL_APP_CONTINUE;
    }
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppIterate(void* state)
{
    uint64_t thisFrameTicks = SDL_GetTicksNS();
    static uint64_t lastFrameTicks = thisFrameTicks;
    double deltaTime = (double)(thisFrameTicks - lastFrameTicks) / 1'000'000'000.0;
    lastFrameTicks = thisFrameTicks;

    Application* application = (Application*)state;
    if (application->Loop(deltaTime))
    {
        return SDL_APP_CONTINUE;
    }
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void* state, SDL_Event* event)
{
    Application* application = (Application*)state;
    application->OnEvent(*event);
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* state, SDL_AppResult /* result */)
{
    Application* application = (Application*)state;
    delete application;
}
