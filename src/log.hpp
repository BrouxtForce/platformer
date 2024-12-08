#pragma once

#include <string>

namespace Log
{
    enum class LogLevel
    {
        Debug, Info, Warn, Error, None
    };

    void SetLogLevel(LogLevel level);

    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);
}
