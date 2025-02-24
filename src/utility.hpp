#pragma once

#include <vector>
#include "data-structures.hpp"

String ReadFile(StringView filepath, MemoryArena* arena);
std::vector<uint8_t> ReadFileBuffer(StringView filepath);

void WriteFile(StringView filepath, StringView data);

// Returns the filenames in a directory
Array<String> GetFilesInDirectory(StringView directory, MemoryArena* arena);

struct CharacterInputStream
{
    StringView input;
    size_t position = 0;

    CharacterInputStream(StringView input);

    char Peek() const;
    char Next();
    bool Eof() const;
};
