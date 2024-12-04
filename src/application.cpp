#include "application.hpp"

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

bool Application::Init()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return false;
    }

    window = SDL_CreateWindow("", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (window == nullptr)
    {
        return false;
    }
    if (!renderer.Init(window))
    {
        return false;
    }
    return true;
}

bool Application::Loop()
{
    return renderer.Render();
}

void Application::Exit()
{
    SDL_DestroyWindow(window);
}
