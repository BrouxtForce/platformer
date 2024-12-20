#include "utility.hpp"

#include <SDL3/SDL.h>

std::string GetBasePath()
{
    static const char* basePath = SDL_GetBasePath();
#if __EMSCRIPTEN__
    return basePath;
#else
    return std::string(basePath) + "../";
#endif
}

std::string ReadFile(const std::string& filepath)
{

    size_t dataSize = 0;
    char* data = (char*)SDL_LoadFile((GetBasePath() + filepath).c_str(), &dataSize);
    if (data == nullptr)
    {
        return {};
    }
    std::string out = data;
    SDL_free(data);
    return out;
}

std::vector<uint8_t> ReadFileBuffer(const std::string& filepath)
{
    std::vector<uint8_t> dataVector;

    size_t dataSize = 0;
    void* data = SDL_LoadFile((GetBasePath() + filepath).c_str(), &dataSize);
    if (data == nullptr)
    {
        return dataVector;
    }

    dataVector.reserve(dataSize);
    SDL_memcpy(dataVector.data(), data, dataSize);

    SDL_free(data);

    return dataVector;
}
