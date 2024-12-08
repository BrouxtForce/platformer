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
    Application* application = (Application*)state;
    if (application->Loop())
    {
        return SDL_APP_CONTINUE;
    }
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void* state, SDL_Event* event)
{
    Application* application = (Application*)state;
    switch (event->type)
    {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            application->OnKeyDown(event->key);
            break;
        case SDL_EVENT_KEY_UP:
            application->OnKeyUp(event->key);
            break;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* state, SDL_AppResult /* result */)
{
    Application* application = (Application*)state;
    delete application;
}
