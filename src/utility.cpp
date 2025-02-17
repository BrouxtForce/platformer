#include "utility.hpp"

#include <SDL3/SDL.h>
#include <cassert>
#include "log.hpp"

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
    Log::Debug("Read '%' (% bytes)", filepath.c_str(), dataSize);

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
    Log::Debug("Read '%' (% bytes)", filepath.c_str(), dataSize);

    dataVector.resize(dataSize);
    SDL_memcpy(dataVector.data(), data, dataSize);

    SDL_free(data);

    return dataVector;
}

void WriteFile(const std::string& filepath, const std::string& data)
{
    const std::string fullPath = GetBasePath() + filepath;
    // TODO: There seems to be an SDL_SaveFile() function in the next release, which would simplify this code
    SDL_IOStream* context = SDL_IOFromFile(fullPath.c_str(), "w+");
    if (context == nullptr)
    {
        Log::Error("Failed to write file '%' (Failed to create handle)", filepath.c_str());
        Log::Error(SDL_GetError());
    }
    else
    {
        size_t bytesWritten = SDL_WriteIO(context, data.data(), data.size());
        if (bytesWritten != data.size())
        {
            Log::Error("Failed to write file '%' (% bytes written)", filepath.c_str(), bytesWritten);
            Log::Error(SDL_GetError());
        }
        else
        {
            Log::Debug("Wrote '%' (% bytes)", filepath.c_str(), bytesWritten);
        }
    }
    SDL_CloseIO(context);
    return;
}

std::vector<std::string> GetFilesInDirectory(const std::string& directory)
{
    std::vector<std::string> filenames;

    SDL_EnumerateDirectoryCallback callback = [](void* userData, const char* /* dirname */, const char* filename)
    {
        std::vector<std::string>& filenames = *(std::vector<std::string>*)userData;
        filenames.push_back(filename);
        return SDL_ENUM_CONTINUE;
    };

    bool success = SDL_EnumerateDirectory((GetBasePath() + directory).c_str(), callback, &filenames);
    if (!success)
    {
        Log::Error(SDL_GetError());
    }

    return filenames;
}

CharacterInputStream::CharacterInputStream(std::string_view input)
    : input(input) {}

char CharacterInputStream::Peek() const
{
    assert(position >= 0 && position < input.size());
    return input[position];
}

char CharacterInputStream::Next()
{
    assert(position >= 0 && position < input.size());
    return input[position++];
}

bool CharacterInputStream::Eof() const
{
    return position >= input.size();
}
