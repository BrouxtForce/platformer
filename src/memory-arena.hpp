#pragma once

#include <cstddef>
#include <type_traits>

size_t Align(size_t bytes, size_t alignment);

enum MemoryArenaFlags : size_t
{
    // Guarantees the memory returned by Alloc() is zeroed
    // This flag should only be set on initialization
    MemoryArenaFlags_ClearToZero = 1 << 0,

    // Prevents MemoryArena from calling any logging functions
    MemoryArenaFlags_NoLog = 1 << 1
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
        static_assert(std::is_trivial_v<T>);
        return (T*)Alloc(sizeof(T) * length, alignof(T));
    }

    // If Realloc() is not referring to the immediately previous allocation, it will simply call Alloc()
    // If the output pointer is different from the input pointer, its data will be copied over
    void* Realloc(void* prevData, size_t prevSize, size_t newSize, size_t alignment);

    template<typename T>
    inline T* Realloc(T* prevData, size_t prevLength, size_t newLength)
    {
        static_assert(std::is_trivial_v<T>);
        return (T*)Realloc(prevData, sizeof(T) * prevLength, sizeof(T) * newLength, alignof(T));
    }

    MemoryArena* GetFooter();

    // We expose information about the arenas in debug mode to make sure things don't get out of hand
    #if DEBUG
        // The total number of arenas that have been initialized and not freed. This includes the new arenas
        // that are automatically allocated when an arena does not have enough space for the next allocation
        static size_t NumActiveArenas;

        // The total amount of memory that all memory arenas are currently taking up. This includes the footer
        // that is placed at the end of the block memory in each MemoryArena to store the next arena on resize
        static size_t TotalAllocationSize;

        size_t GetActualSize() const;

        struct MemoryInfo
        {
            size_t usedMemory = 0;
            size_t allocatedMemory = 0;
        };
        MemoryInfo GetMemoryInfo() const;
    #endif
};
