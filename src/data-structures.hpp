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

inline constexpr size_t GetCStringLength(const char* str)
{
    if (std::is_constant_evaluated())
    {
        if (*str == '\0')
        {
            return 0;
        }
        return GetCStringLength(str + 1) + 1;
    }
    return strlen(str);
}

struct StringView;

struct String : Array<char>
{
    static constexpr size_t NPOS = -1;

    void Append(StringView str);
    void Append(char c);

    void Append(unsigned short num);
    void Append(unsigned int num);
    void Append(unsigned long num);
    void Append(unsigned long long num);

    void Append(short num);
    void Append(int num);
    void Append(long num);
    void Append(long long num);

    void Append(double num);

    void Clear();

    bool Equals(StringView str) const;

    String operator+(StringView other) const;
    String operator+(char c) const;

    template<typename T>
    inline void operator+=(const T& other)
    {
        Append(other);
    }

    template<typename T>
    inline String& operator<<(const T& other)
    {
        Append(other);
        return *this;
    }

    bool operator==(StringView str) const;
    bool operator!=(StringView str) const;
};

struct StringView
{
    const char* data = nullptr;
    size_t size = 0;

    StringView() = default;
    StringView(const String& str);

    inline constexpr StringView(const char* str)
    {
        data = str;
        size = GetCStringLength(str);
        if (std::is_constant_evaluated())
        {
            GetCStringLength(str);
        }
        else
        {
            size = strlen(str);
        }
    }

    StringView Substr(size_t position, size_t length = String::NPOS) const;

    bool Equals(StringView str) const;

    bool operator==(StringView str) const;
    bool operator!=(StringView str) const;

    const char& operator[](size_t index) const;
};
