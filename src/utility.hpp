#pragma once

#include <string>
#include <vector>

std::string ReadFile(const std::string& filepath);
std::vector<uint8_t> ReadFileBuffer(const std::string& filepath);

void WriteFile(const std::string& filepath, const std::string& data);

// Returns the filenames in a directory
std::vector<std::string> GetFilesInDirectory(const std::string& directory);
