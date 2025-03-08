#include "log.hpp"

#if __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/console.h>
#else
#include <cstdio>
#endif

#include "utility.hpp"

namespace Log
{
    static MemoryArena s_Arena;

    MemoryArena* GetArena()
    {
        if (s_Arena.data == nullptr)
        {
            // TODO: Is this a good default?
            size_t defaultSize = 1024;

            // We have to use MemoryArenaFlags_NoLog because the Init() function calls Log::Debug()
            // We can remove this flag after initialization to get logs about future allocations
            // TODO: Is there a better way to do this?
            s_Arena.Init(defaultSize, MemoryArenaFlags_NoLog);

            Debug("[LOG] No transient memory arena set. Creating a default memory arena (% bytes)", defaultSize);
            assert(s_Arena.offset == 0);
        }
        return &s_Arena;
    }

#ifndef __EMSCRIPTEN__
    constexpr static StringView ESCAPE_COLOR_CYAN = "\x1b[36m";
    constexpr static StringView ESCAPE_COLOR_YELLOW = "\x1b[33m";
    constexpr static StringView ESCAPE_COLOR_RED = "\x1b[31m";
    constexpr static StringView ESCAPE_COLOR_RESET = "\x1b[0m";

    constexpr static StringView DEBUG_PREFIX = "[DEBUG] ";
    constexpr static StringView INFO_PREFIX = "[INFO] ";
    constexpr static StringView WARN_PREFIX = "[WARN] ";
    constexpr static StringView ERROR_PREFIX = "[ERROR] ";
#endif

    static LogLevel s_LogLevel = LogLevel::Info;
    void SetLogLevel(LogLevel level)
    {
        s_LogLevel = level;
    }

    void Debug(StringView message)
    {
        if (s_LogLevel > LogLevel::Debug)
        {
            return;
        }

        String out;
        out.arena = GetArena();
#if __EMSCRIPTEN__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
        out << message << '\0';
        EM_ASM({
            console.debug(UTF8ToString($0));
        }, out.data);
#pragma clang diagnostic pop
#else
        out << ESCAPE_COLOR_CYAN << DEBUG_PREFIX << message << ESCAPE_COLOR_RESET << '\n';
        fwrite(out.data, sizeof(char), out.size, stdout);
        fflush(stdout);
#endif
        s_Arena.Clear();
    }

    void Info(StringView message)
    {
        if (s_LogLevel > LogLevel::Info)
        {
            return;
        }

        String out;
        out.arena = GetArena();
#if __EMSCRIPTEN__
        out << message << '\0';
        emscripten_console_log(out.data);
#else
        out << INFO_PREFIX << message << ESCAPE_COLOR_RESET << '\n';
        fwrite(out.data, sizeof(char), out.size, stdout);
        fflush(stdout);
#endif
        s_Arena.Clear();
    }

    void Warn(StringView message)
    {
        if (s_LogLevel > LogLevel::Warn)
        {
            return;
        }

        String out;
        out.arena = GetArena();
#if __EMSCRIPTEN__
        out << message << '\0';
        emscripten_console_warn(out.data);
#else
        out << ESCAPE_COLOR_YELLOW << WARN_PREFIX << message << ESCAPE_COLOR_RESET << '\n';
        fwrite(out.data, sizeof(char), out.size, stdout);
        fflush(stdout);
#endif
        s_Arena.Clear();
    }

    void Error(StringView message)
    {
        Breakpoint();

        if (s_LogLevel > LogLevel::Error)
        {
            return;
        }

        String out;
        out.arena = GetArena();
#if __EMSCRIPTEN__
        out << message << '\0';
        emscripten_console_error(out.data);
#else
        out << ESCAPE_COLOR_RED << ERROR_PREFIX << message << ESCAPE_COLOR_RESET << '\n';
        fwrite(out.data, sizeof(char), out.size, stderr);
#endif
        s_Arena.Clear();
    }
}
