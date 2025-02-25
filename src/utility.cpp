#include "utility.hpp"

#include <SDL3/SDL.h>
#include <cassert>
#include "log.hpp"
#include "application.hpp"

StringView GetBasePath()
{
    static String basePath;
    if (basePath.data == nullptr)
    {
        basePath.arena = &GlobalArena;
        basePath += SDL_GetBasePath();
#ifndef __EMSCRIPTEN__
        basePath += "../";
#endif
    }
    return basePath;
}

// Returns a null-terminated string for convenience
StringView GetFullPath(StringView filepath, MemoryArena* arena)
{
    String out;
    out.arena = arena;
    out << GetBasePath() << filepath << '\0';
    return out;
}

String ReadFile(StringView filepath, MemoryArena* arena)
{
    StringView fullPath = GetFullPath(filepath, &TransientArena);

    size_t dataSize = 0;
    char* data = (char*)SDL_LoadFile(fullPath.data, &dataSize);
    if (data == nullptr)
    {
        return {};
    }
    Log::Debug("Read '%' (% bytes)", filepath, dataSize);

    String out;
    out.arena = arena;
    out += StringView(data, dataSize);
    SDL_free(data);

    return out;
}

Array<uint8_t> ReadFileBuffer(StringView filepath, MemoryArena* arena)
{
    StringView fullPath = GetFullPath(filepath, &TransientArena);

    Array<uint8_t> out;
    out.arena = arena;

    size_t dataSize = 0;
    void* data = SDL_LoadFile(fullPath.data, &dataSize);
    if (data == nullptr)
    {
        return out;
    }
    Log::Debug("Read '%' (% bytes)", filepath, dataSize);

    out.Resize(dataSize);
    memcpy(out.data, data, dataSize);

    SDL_free(data);

    return out;
}

void WriteFile(StringView filepath, StringView data)
{
    StringView fullPath = GetFullPath(filepath, &TransientArena);
    // TODO: There seems to be an SDL_SaveFile() function in the next release, which would simplify this code
    SDL_IOStream* context = SDL_IOFromFile(fullPath.data, "w+");
    if (context == nullptr)
    {
        Log::Error("Failed to write file '%' (Failed to create handle)", filepath);
        Log::Error(SDL_GetError());
    }
    else
    {
        size_t bytesWritten = SDL_WriteIO(context, data.data, data.size);
        if (bytesWritten != data.size)
        {
            Log::Error("Failed to write file '%' (% bytes written)", filepath, bytesWritten);
            Log::Error(SDL_GetError());
        }
        else
        {
            Log::Debug("Wrote '%' (% bytes)", filepath, bytesWritten);
        }
    }
    SDL_CloseIO(context);
    return;
}

Array<String> GetFilesInDirectory(StringView directory, MemoryArena* arena)
{
    Array<String> filenames;
    filenames.arena = arena;

    // TODO: This is arbitrary, and is likely more than needed in most cases
    filenames.Reserve(64);

    SDL_EnumerateDirectoryCallback callback = [](void* userData, const char* /* dirname */, const char* filename)
    {
        Array<String>& filenames = *(Array<String>*)userData;

        String str;
        str.arena = filenames.arena;
        str.Append(filename);

        filenames.Push(str);
        return SDL_ENUM_CONTINUE;
    };

    StringView fullPath = GetFullPath(directory, &TransientArena);
    bool success = SDL_EnumerateDirectory(fullPath.data, callback, &filenames);
    if (!success)
    {
        Log::Error(SDL_GetError());
    }

    return filenames;
}

CharacterInputStream::CharacterInputStream(StringView input)
    : input(input) {}

char CharacterInputStream::Peek() const
{
    assert(position >= 0 && position < input.size);
    return input[position];
}

char CharacterInputStream::Next()
{
    assert(position >= 0 && position < input.size);
    return input[position++];
}

bool CharacterInputStream::Eof() const
{
    return position >= input.size;
}
