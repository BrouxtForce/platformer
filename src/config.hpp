#pragma once

#include <optional>

#include "data-structures.hpp"
#include "transform.hpp"

namespace Config
{
    void SetMemoryArena(MemoryArena* arena);

    void Load(StringView filepath);
    void LoadRaw(StringView text);

    bool HasKey(StringView key);

    template<typename T>
    std::optional<T> Get(StringView key);

    template<typename T>
    inline T Get(StringView key, T defaultValue)
    {
        return Get<T>(key).value_or(defaultValue);
    }

    Array<StringView> GetTables(MemoryArena* arena);
    Array<StringView> GetKeys(MemoryArena* arena);

    void PushTable(StringView name);
    void PopTable();

    void SuppressWarnings(bool value);
}
