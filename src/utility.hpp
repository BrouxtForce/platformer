#pragma once

#include <string>
#include <vector>

std::string ReadFile(const std::string& filepath);
std::vector<uint8_t> ReadFileBuffer(const std::string& filepath);

void WriteFile(const std::string& filepath, const std::string& data);

// Returns the filenames in a directory
std::vector<std::string> GetFilesInDirectory(const std::string& directory);

struct CharacterInputStream
{
    std::string_view input;
    size_t position = 0;

    CharacterInputStream(std::string_view input);

    char Peek() const;
    char Next();
    bool Eof() const;
};
