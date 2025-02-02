#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <functional>

namespace Log
{
    template<typename... Args>
    inline std::string Format(std::string_view message, const Args&... args)
    {
        std::stringstream ss;

        size_t charactersWritten = 0;
        bool argMismatch = false;

        size_t expectedArgCount = 0;
        size_t receivedArgCount = 0;

        std::function<size_t()> FindNextInsertion = [&] ()
        {
            size_t insertPos = std::string::npos;
            for (int i = charactersWritten; i < (int)message.size(); i++)
            {
                if (message[i] == '%')
                {
                    if (i != 0 && message[i - 1] == '\\')
                    {
                        ss << message.substr(charactersWritten, i - charactersWritten - 1);
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
            if (insertPos == std::string::npos)
            {
                argMismatch = true;
                return;
            }
            ss << message.substr(charactersWritten, insertPos - charactersWritten) << args;
            charactersWritten = insertPos + 1;
        } (), ...);

        while (true)
        {
            size_t insertPos = FindNextInsertion();
            if (insertPos == std::string::npos)
            {
                ss << message.substr(charactersWritten);
                break;
            }
            ss << message.substr(charactersWritten, insertPos - charactersWritten);
            charactersWritten = insertPos + 1;
            argMismatch = true;
        }
        if (argMismatch)
        {
            std::cerr << "Format string arg mismatch: Expected " << expectedArgCount << " args but received " << receivedArgCount
                      << ". Message: '" << message << "'\n";
        }

        return ss.str();
    }

    enum class LogLevel
    {
        Debug, Info, Warn, Error, None
    };

    void SetLogLevel(LogLevel level);

    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);

    template<typename... Args>
    inline void Debug(const std::string& message, const Args&... args)
    {
        Debug(Format(message, args...));
    }

    template<typename... Args>
    inline void Info(const std::string& message, const Args&... args)
    {
        Info(Format(message, args...));
    }

    template<typename... Args>
    inline void Warn(const std::string& message, const Args&... args)
    {
        Warn(Format(message, args...));
    }

    template<typename... Args>
    inline void Error(const std::string& message, const Args&... args)
    {
        Error(Format(message, args...));
    }
}
