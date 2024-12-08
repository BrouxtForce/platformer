#include "log.hpp"
#include <complex.h>

#if __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/console.h>
#else
#include <iostream>
#endif

namespace Log
{
#ifndef __EMSCRIPTEN__
    constexpr static const char* ESCAPE_COLOR_CYAN = "\x1b[36m";
    constexpr static const char* ESCAPE_COLOR_YELLOW = "\x1b[33m";
    constexpr static const char* ESCAPE_COLOR_RED = "\x1b[31m";
    constexpr static const char* ESCAPE_COLOR_RESET = "\x1b[0m";
#endif

    static LogLevel s_LogLevel = LogLevel::Info;
    void SetLogLevel(LogLevel level)
    {
        s_LogLevel = level;
    }

    void Debug(const std::string& message)
    {
        if (s_LogLevel > LogLevel::Debug)
        {
            return;
        }
#if __EMSCRIPTEN__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
        EM_ASM({
            console.debug(UTF8ToString($0));
        }, message.c_str());
#pragma clang diagnostic pop
#else
        std::cout << ESCAPE_COLOR_CYAN << "[DEBUG] " << message << ESCAPE_COLOR_RESET << std::endl;
#endif
    }

    void Info(const std::string& message)
    {
        if (s_LogLevel > LogLevel::Info)
        {
            return;
        }
#if __EMSCRIPTEN__
        emscripten_console_log(message.c_str());
#else
        std::cout << "[INFO] " << message << std::endl;
#endif
    }

    void Warn(const std::string& message)
    {
        if (s_LogLevel > LogLevel::Warn)
        {
            return;
        }
#if __EMSCRIPTEN__
        emscripten_console_warn(message.c_str());
#else
        std::cout << ESCAPE_COLOR_YELLOW << "[WARN] " << message << ESCAPE_COLOR_RESET << std::endl;
#endif
    }

    void Error(const std::string& message)
    {
        if (s_LogLevel > LogLevel::Error)
        {
            return;
        }
#if __EMSCRIPTEN__
        emscripten_console_error(message.c_str());
#else
        std::cerr << ESCAPE_COLOR_RED << "[ERROR] " << message << ESCAPE_COLOR_RESET << std::endl;
#endif
    }
}
