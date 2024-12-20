#pragma once

#include <string>
#include <vector>

std::string ReadFile(const std::string& filepath);
std::vector<uint8_t> ReadFileBuffer(const std::string& filepath);
