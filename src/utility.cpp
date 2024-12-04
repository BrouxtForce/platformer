#include "utility.hpp"

#include <SDL3/SDL.h>

std::string ReadFile(const std::string& filepath)
{
    size_t dataSize = 0;
    char* data = (char*)SDL_LoadFile(filepath.c_str(), &dataSize);
    if (data == nullptr)
    {
        return {};
    }
    std::string out = data;
    SDL_free(data);
    return out;
}
