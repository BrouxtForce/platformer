#pragma once

#include <cstddef>
#include <type_traits>
#include <cassert>
#include <cstring>
#include <bit>
#include <string_view>

#include "memory-arena.hpp"
#include "math.hpp"

template<typename T>
struct PointerIterator
{
    T* ptr = nullptr;

    constexpr T& operator*() const
    {
        return *ptr;
    }
    constexpr T& operator++()
    {
        ptr++;
        return *ptr;
    }
    constexpr bool operator==(PointerIterator<T> other)
    {
        return ptr == other.ptr;
    };
};

template<typename T>
struct Array
{
    static_assert(std::is_trivially_destructible_v<T>);

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

    PointerIterator<T> begin()
    {
        return { .ptr = data };
    }
    PointerIterator<T> end()
    {
        return { .ptr = data + size };
    }
    PointerIterator<const T> begin() const
    {
        return { .ptr = data };
    }
    PointerIterator<const T> end() const
    {
        return { .ptr = data + size };
    }
};

template<typename T>
struct Span
{
    T* data = nullptr;
    size_t size = 0;

    constexpr Span() = default;

    constexpr Span(const Array<T>& array)
    {
        data = array.data;
        size = array.size;
    }

    constexpr Span(T* data, size_t size)
        : data(data), size(size) {}

    inline Span<T> Slice(size_t start, size_t length = SIZE_MAX) const
    {
        return {
            data + start,
            Math::Min(size - start, length)
        };
    }

    inline T& operator[](size_t index) const
    {
        assert(index >= 0 && index < size);
        return data[index];
    }

    inline bool operator==(const Span<T>& other) const
    {
        if (size != other.size)
        {
            return false;
        }
        return memcmp(data, other.data, size) == 0;
    }

    constexpr PointerIterator<T> begin() const
    {
        return { .ptr = data };
    }
    constexpr PointerIterator<T> end() const
    {
        return { .ptr = data + size };
    }
};

/*
    A dynamic array with the following properties:
    - Pointers returned by Push() are stable
    - Calling Erase(ptr) frees up the slot used by ptr
    - The order of the array is not necessarily the order of insertion
    - Push() can insert new elements before the last element
    - All elements can be iteratod through with a range-based for loop
*/

template<typename T>
struct StableArray
{
    static_assert(std::is_trivially_destructible_v<T>);

    MemoryArena* arena = nullptr;

    T* data = nullptr;
    StableArray* next = nullptr;
    uint64_t allocatedMask = 0;
    constexpr static size_t BlockSize = 64;

    inline void Init()
    {
        size_t dataSize = Align(64 * sizeof(T), alignof(decltype(*this)));
        assert(arena->flags & MemoryArenaFlags_ClearToZero);
        data = (T*)arena->Alloc(
            dataSize + sizeof(*this),
            Math::Max(alignof(T), alignof(decltype(*this)))
        );
        next = (StableArray<T>*)((char*)data + dataSize);
        next->arena = arena;
    }

    inline T* Push(T value)
    {
        if (data == nullptr)
        {
            Init();
        }
        int nextFree = std::countr_one(allocatedMask);

        T* result = nullptr;
        if (nextFree == 64)
        {
            result = next->Push(value);
        }
        else
        {
            allocatedMask |= ((uint64_t)1 << nextFree);
            result = data + nextFree;
        }
        *result = value;
        return result;
    }

    inline void Erase(T* ptr)
    {
        if (ptr >= data && ptr < data + 64)
        {
            int index = ptr - data;
            allocatedMask &= ~((uint64_t)1 << index);
        }
        else
        {
            assert(next != nullptr);
            next->Erase(ptr);
        }
    }

    inline void Clear()
    {
        allocatedMask = 0;
        if (next != nullptr)
        {
            next->Clear();
        }
    }

    struct Iterator
    {
        const StableArray<T>* currentArray = nullptr;
        uint64_t currentMask = 0;

        inline T& operator*() const
        {
            assert(currentArray != nullptr && currentMask != 0);
            int next = std::countr_zero(currentMask);
            return currentArray->data[next];
        }
        inline T& operator++()
        {
            assert(currentArray != nullptr && currentMask != 0);
            int next = std::countr_zero(currentMask);
            currentMask &= ~((uint64_t)1 << next);
            T* result = &currentArray->data[next];
            NextNonEmpty();
            return *result;
        }
        inline bool operator==(Iterator other)
        {
            return currentArray == other.currentArray && currentMask == other.currentMask;
        };
        inline void NextNonEmpty()
        {
            while (currentMask == 0)
            {
                currentArray = currentArray->next;
                if (currentArray == nullptr)
                {
                    currentMask = 0;
                    break;
                }
                currentMask = currentArray->allocatedMask;
                if (currentArray->data == nullptr)
                {
                    currentArray = nullptr;
                    currentMask = 0;
                    break;
                }
            }
        }
    };
    inline Iterator begin() const
    {
        Iterator iterator { .currentArray = this, .currentMask = allocatedMask };
        iterator.NextNonEmpty();
        return iterator;
    };
    inline Iterator end() const
    {
        return {};
    };
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

    void Append(Math::float2 vec);
    void Append(Math::float3 vec);

    void NullTerminate();

    void ReplaceAll(char oldValue, char newValue);

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

    static String Copy(StringView str, MemoryArena* arena);
};

struct StringView
{
    const char* data = nullptr;
    size_t size = 0;

    StringView() = default;
    StringView(const String& str);

    constexpr StringView(const char* str)
    {
        data = str;
        if (std::is_constant_evaluated())
        {
            size = GetCStringLength(str);
        }
        else
        {
            size = strlen(str);
        }
    }

    constexpr StringView(const char* str, size_t length)
    {
        data = str;
        size = length;
    }

    constexpr StringView(const char& c)
    {
        data = &c;
        size = 1;
    }

    StringView Substr(size_t position, size_t length = String::NPOS) const;

    bool Equals(StringView str) const;
    bool StartsWith(StringView prefix) const;
    bool EndsWith(StringView suffix) const;

    size_t Find(char c, size_t start) const;

    int ToInt();
    float ToFloat();

    bool operator==(StringView str) const;
    bool operator!=(StringView str) const;

    const char& operator[](size_t index) const;

    constexpr PointerIterator<const char> begin() const
    {
        return { .ptr = data };
    }
    constexpr PointerIterator<const char> end() const
    {
        return { .ptr = data + size };
    }
};

// TODO: Remove reliance on std::string_view
template<>
struct std::hash<StringView>
{
    size_t operator()(const StringView& view) const
    {
        return std::hash<std::string_view>{}(std::string_view(view.data, view.size));
    }
};
