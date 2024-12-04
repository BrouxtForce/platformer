#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "application.hpp"

SDL_AppResult SDL_AppInit(void** state, int /* argc */, char** /* argv */)
{
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

SDL_AppResult SDL_AppEvent(void* /* state */, SDL_Event* event)
{
    switch (event->type)
    {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* state, SDL_AppResult /* result */)
{
    Application* application = (Application*)state;
    delete application;
}
