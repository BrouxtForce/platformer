#include "memory-arena.hpp"

#include <memory>
#include <algorithm>
#include <cassert>
#include <bit>

#include "log.hpp"

size_t Align(size_t bytes, size_t alignment)
{
    assert(std::has_single_bit(alignment));
    return bytes + (alignment - bytes % alignment) % alignment;
}

void MemoryArena::Init(size_t bytes, size_t flags)
{
    assert(data == nullptr && "Free() must be called before calling Init() again");

    // We store memory for the next MemoryArena at the end of the allocated block
    // We pretend this memory doesn't exist so we don't return allocations that intrude this space
    size_t requestedSize = Align(bytes, alignof(MemoryArena));
    size_t actualSize = requestedSize + sizeof(MemoryArena);

    size = requestedSize;
    offset = 0;
    this->flags = flags;

    if (flags & MemoryArenaFlags_ClearToZero)
    {
        data = calloc(actualSize, 1);
    }
    else
    {
        data = malloc(actualSize);

        // The memory for the next arena should still be zeroed
        memset(GetFooter(), 0, sizeof(MemoryArena));
    }

    if (data == nullptr)
    {
        std::abort();
    }

#if DEBUG
    NumActiveArenas++;
    TotalAllocationSize += actualSize;
#endif

    if ((flags & MemoryArenaFlags_NoLog) == 0)
    {
        Log::Debug("Memory Arena Allocation (% bytes)", actualSize);
    }
}

void MemoryArena::Clear()
{
    if (flags & MemoryArenaFlags_ClearToZero)
    {
        memset(data, 0, offset);
    }
    offset = 0;
    if (next != nullptr)
    {
        next->Clear();
    }
}

void MemoryArena::Free()
{
    if (next != nullptr)
    {
        next->Free();
    }

    free(data);

#if DEBUG
    NumActiveArenas--;
    TotalAllocationSize -= size + sizeof(MemoryArena);
#endif

    data = nullptr;
    size = 0;
    offset = 0;

}

void* MemoryArena::Alloc(size_t bytes, size_t alignment)
{
    assert(data != nullptr && bytes > 0);

    void* result = nullptr;

    size_t newOffset = Align(offset, alignment);
    if (newOffset + bytes > size)
    {
        if (next == nullptr)
        {
            // TODO: Is this arena's current size a good minimum size?
            MemoryArena* footer = GetFooter();
            footer->Init(std::max(size, bytes), flags);

            next = footer;
        }
        result = next->Alloc(bytes, alignment);
    }
    else
    {
        result = (char*)data + newOffset;
        offset = newOffset + bytes;
    }

    assert(result != nullptr);

    return result;
}

void* MemoryArena::Realloc(void* prevData, size_t prevSize, size_t newSize, size_t alignment)
{
    assert(data != nullptr && prevData != nullptr);

    if (prevSize == newSize)
    {
        if ((flags & MemoryArenaFlags_NoLog) == 0)
        {
            Log::Warn("Redundant realloc! (% bytes)", newSize);
        }
        return prevData;
    }

    // If it's not in this block, try the next one
    if (prevData < data || prevData >= (char*)data + size)
    {
        assert(next != nullptr);
        return next->Realloc(prevData, prevSize, newSize, alignment);
    }

    // If it was the immediately previous allocation, dealloc
    if ((char*)prevData + prevSize == (char*)data + offset)
    {
        offset -= prevSize;
    }

    void* newData = Alloc(newSize, alignment);
    if (newData == prevData)
    {
        if (newSize < prevSize && (flags & MemoryArenaFlags_ClearToZero))
        {
            memset((char*)newData + newSize, 0, prevSize - newSize);
        }
        return newData;
    }

    memcpy(newData, prevData, std::min(prevSize, newSize));
    if (flags & MemoryArenaFlags_ClearToZero)
    {
        memset(prevData, 0, prevSize);
    }

    return newData;
}

MemoryArena* MemoryArena::GetFooter()
{
    return (MemoryArena*)((char*)data + size);
}

#if DEBUG
    size_t MemoryArena::NumActiveArenas = 0;
    size_t MemoryArena::TotalAllocationSize = 0;

    MemoryArena::MemoryInfo MemoryArena::GetMemoryInfo() const
    {
        // We don't include the footer in the memory info
        // TODO: Should we?
        size_t usedMemory = offset;
        size_t allocatedMemory = size;
        if (next != nullptr)
        {
            MemoryInfo info = next->GetMemoryInfo();
            allocatedMemory += info.allocatedMemory;
            usedMemory += info.usedMemory;
        }
        return {
            .usedMemory = usedMemory,
            .allocatedMemory = allocatedMemory
        };
    }
#endif
