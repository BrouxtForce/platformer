#pragma once

#include <cstddef>
#include <type_traits>
#include <cassert>
#include <cstring>

#include "memory-arena.hpp"

template<typename T>
struct Array
{
    static_assert(std::is_trivially_constructible<T>() && std::is_trivially_destructible<T>());

    MemoryArena* arena = nullptr;

    T* data = nullptr;
    size_t size = 0;
    size_t capacity = 0;

    inline void Reserve(size_t newCapacity)
    {
        assert(arena != nullptr);
        if (newCapacity <= capacity)
        {
            return;
        }
        if (data == nullptr)
        {
            data = arena->Alloc<T>(newCapacity);
            capacity = newCapacity;
            return;
        }
        data = arena->Realloc<T>(data, capacity, newCapacity);
        capacity = newCapacity;
    }

    inline void Resize(size_t newSize)
    {
        if (newSize == size) return;
        if (newSize < size)
        {
            memset(data + newSize, 0, (size - newSize) * sizeof(T));
            size = newSize;
            return;
        }
        Reserve(newSize);
        size = newSize;
    }

    inline void Push(T value)
    {
        if (capacity == 0)
        {
            // TODO: What amount should we reserve here? It is unlikely that the array won't surpass length one
            Reserve(1);
        }
        if (size + 1 >= capacity)
        {
            Reserve(capacity * 2);
        }
        data[size++] = value;
    }

    inline void Pop()
    {
        assert(size > 0);
        size--;
        data[size] = 0;
    }

    inline T& operator[](size_t index)
    {
        assert(index >= 0 && index < size);
        return data[index];
    }
};

struct StringView;

struct String : Array<char>
{
    void Append(StringView str);
    void Append(char c);

    bool Equals(StringView str) const;

    String operator+(StringView other) const;
    String operator+(char c) const;

    void operator+=(StringView other);
    void operator+=(char c);

    bool operator==(StringView str) const;
    bool operator!=(StringView str) const;
};

struct StringView
{
    const char* data = nullptr;
    size_t size = 0;

    StringView() = default;
    StringView(const String& str);
    StringView(const char* str);

    bool Equals(StringView str) const;

    bool operator==(StringView str) const;
    bool operator!=(StringView str) const;
};
