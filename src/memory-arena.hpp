#pragma once

#include <cstddef>
#include <type_traits>

enum MemoryArenaFlags : size_t
{
    // Guarantees the memory returned by Alloc() is zeroed
    // This flag should only be set on initialization
    MemoryArenaFlags_ClearToZero = 1 << 0
};

// TODO: The current implementation uses malloc() and free(), but maybe we can take advantage
//   of 64-bit address spaces to rid the need of a linked-list of arenas for growing (e.g. use
//   VirtualAlloc() or mmap() and reserve a large portion of virtual memory)

// NOTE: This arena is NOT thread safe

struct MemoryArena
{
    void* data = nullptr;
    size_t size = 0;
    size_t offset = 0;
    size_t flags = 0;

    MemoryArena* next = nullptr;

    void Init(size_t bytes, size_t flags);
    void Clear();
    void Free();

    void* Alloc(size_t bytes, size_t alignment);

    template<typename T>
    inline T* Alloc(size_t length = 1)
    {
        // Constructors/destructors will not be called! That would defeat the purpose of the arena allocator
        static_assert(std::is_trivially_constructible<T>() && std::is_trivially_destructible<T>());
        return (T*)Alloc(sizeof(T) * length, alignof(T));
    }

    // If Realloc() is not referring to the immediately previous allocation, it will simply call Alloc()
    // If the output pointer is different from the input pointer, its data will be copied over
    void* Realloc(void* prevData, size_t prevSize, size_t newSize, size_t alignment);

    template<typename T>
    inline T* Realloc(T* prevData, size_t prevLength, size_t newLength)
    {
        static_assert(std::is_trivially_constructible<T>() && std::is_trivially_destructible<T>());
        return (T*)Realloc(prevData, sizeof(T) * prevLength, sizeof(T) * newLength, alignof(T));
    }

    MemoryArena* GetFooter();
};
