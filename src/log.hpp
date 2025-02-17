#pragma once

#include <functional>

#include "data-structures.hpp"

namespace Log
{
    MemoryArena* GetArena();

    enum class LogLevel
    {
        Debug, Info, Warn, Error, None
    };

    void SetLogLevel(LogLevel level);

    void Debug(StringView message);
    void Info(StringView message);
    void Warn(StringView message);
    void Error(StringView message);

    template<typename... Args>
    inline String Format(MemoryArena* arena, StringView message, const Args&... args)
    {
        String output;
        output.arena = arena;

        size_t charactersWritten = 0;
        bool argMismatch = false;

        size_t expectedArgCount = 0;
        size_t receivedArgCount = 0;

        std::function<size_t()> FindNextInsertion = [&] ()
        {
            size_t insertPos = String::NPOS;
            for (size_t i = charactersWritten; i < message.size; i++)
            {
                if (message[i] == '%')
                {
                    if (i != 0 && message[i - 1] == '\\')
                    {
                        output += message.Substr(charactersWritten, i - charactersWritten - 1);
                        charactersWritten = i;
                        continue;
                    }
                    expectedArgCount++;
                    insertPos = i;
                    break;
                }
            }
            return insertPos;
        };

        ([&]
        {
            receivedArgCount++;

            size_t insertPos = FindNextInsertion();
            if (insertPos == String::NPOS)
            {
                argMismatch = true;
                return;
            }
            output << message.Substr(charactersWritten, insertPos - charactersWritten) << args;
            charactersWritten = insertPos + 1;
        } (), ...);

        while (true)
        {
            size_t insertPos = FindNextInsertion();
            if (insertPos == String::NPOS)
            {
                output += message.Substr(charactersWritten);
                break;
            }
            output += message.Substr(charactersWritten, insertPos - charactersWritten);
            charactersWritten = insertPos + 1;
            argMismatch = true;
        }
        if (argMismatch)
        {
            String errorMessage = Format(GetArena(), "Format string arg mismatch: Expected % args but received %. Message: '%'", expectedArgCount, receivedArgCount, message);
            Error(errorMessage);
        }

        return output;
    }

    template<typename... Args>
    inline void Debug(StringView message, const Args&... args)
    {
        Debug(Format(GetArena(), message, args...));
        GetArena()->Clear();
    }

    template<typename... Args>
    inline void Info(StringView message, const Args&... args)
    {
        Info(Format(GetArena(), message, args...));
        GetArena()->Clear();
    }

    template<typename... Args>
    inline void Warn(StringView message, const Args&... args)
    {
        Warn(Format(GetArena(), message, args...));
        GetArena()->Clear();
    }

    template<typename... Args>
    inline void Error(StringView message, const Args&... args)
    {
        Error(Format(GetArena(), message, args...));
        GetArena()->Clear();
    }
}
