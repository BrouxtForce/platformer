#include "memory-arena.hpp"
#include <doctest.h>

TEST_CASE("Memory Arena")
{
    MemoryArena arena;
    arena.Init(64, MemoryArenaFlags_ClearToZero);

    SUBCASE("Allocations should be non-overlapping without unnecessarily wasting space")
    {
        uint8_t* allocations[64];
        for (uint8_t i = 0; i < 64; i++)
        {
            allocations[i] = arena.Alloc<uint8_t>(1);
            *allocations[i] = i;
        }

        CHECK(arena.next == nullptr);
        for (int i = 0; i < 64; i++)
        {
            CHECK(*allocations[i] == i);
        }
    }

    SUBCASE("Allocating more memory than the initial size")
    {
        constexpr size_t ALLOCATION_SIZE = 128;
        uint8_t* array = arena.Alloc<uint8_t>(ALLOCATION_SIZE);

        // The smaller arena should be left unchanged
        CHECK(arena.offset == 0);

        // The expected properties of the next arena:
        REQUIRE(arena.next != nullptr);
        CHECK(arena.next->size == ALLOCATION_SIZE);
        CHECK(arena.next->next == nullptr);
        CHECK(arena.next->offset == arena.next->size);
        CHECK(arena.next->flags == arena.flags);
    }

    SUBCASE("Clear() should zero previously allocated memory")
    {
        uint64_t* first_a = arena.Alloc<uint64_t>();
        uint32_t* first_b = arena.Alloc<uint32_t>();

        CHECK(*first_a == 0);
        CHECK(*first_b == 0);

        *first_a = ~(uint64_t)1;
        *first_b = ~(uint32_t)1;

        arena.Clear();

        uint64_t* second_a = arena.Alloc<uint64_t>();
        uint32_t* second_b = arena.Alloc<uint32_t>();

        CHECK(first_a == second_a);
        CHECK(first_b == second_b);

        CHECK(*second_a == 0);
        CHECK(*second_b == 0);
    }

    SUBCASE("Clear() should zero all sub-arenas")
    {
        uint8_t* a = arena.Alloc<uint8_t>(arena.size);
        uint8_t* b = arena.Alloc<uint8_t>(arena.size);
        uint8_t* c = arena.Alloc<uint8_t>(arena.size);

        // We should have two sub-arenas of the same size after these allocations
        REQUIRE(arena.next != nullptr);
        REQUIRE(arena.next->next != nullptr);
        CHECK(arena.next->next->next == nullptr);

        for (int i = 0; i < arena.size; i++)
        {
            a[i] = ~(uint8_t)0;
            b[i] = ~(uint8_t)0;
            c[i] = ~(uint8_t)0;
        }

        arena.Clear();

        for (int i = 0; i < arena.size; i++)
        {
            CHECK(a[i] == 0);
            CHECK(b[i] == 0);
            CHECK(c[i] == 0);
        }
    }

    SUBCASE("Realloc()")
    {
        SUBCASE("Reallocing a greater piece of memory")
        {
            constexpr size_t ALLOCATION_SIZE = 64;

            uint8_t* a = arena.Alloc<uint8_t>(ALLOCATION_SIZE);
            for (int i = 0; i < ALLOCATION_SIZE; i++)
            {
                a[i] = i + 1;
            }

            uint8_t* b = arena.Realloc<uint8_t>(a, ALLOCATION_SIZE, ALLOCATION_SIZE * 2);
            for (int i = 0; i < ALLOCATION_SIZE; i++)
            {
                // Previous memory should be zeroed
                CHECK(a[i] == 0);

                // New memory should have proper data
                CHECK(b[i] == i + 1);
            }

            b = arena.Alloc<uint8_t>(ALLOCATION_SIZE);
            // The initial allocation should have been freed
            CHECK(a == b);
        }

        SUBCASE("Reallocing a smaller piece of memory")
        {
            uint8_t* a = arena.Alloc<uint8_t>(8);
            for (int i = 0; i < 8; i++)
            {
                a[i] = i + 1;
            }

            uint8_t* b = arena.Realloc<uint8_t>(a, 8, 4);
            for (int i = 0; i < 4; i++)
            {
                CHECK(a[i] == i + 1);
            }

            uint8_t* c = arena.Realloc<uint8_t>(b, 4, 8);
            for (int i = 0; i < 8; i++)
            {
                if (i < 4)
                {
                    CHECK(a[i] == i + 1);
                }
                else
                {
                    CHECK(a[i] == 0);
                }
            }

            CHECK(a == b);
            CHECK(b == c);
        }

        SUBCASE("Realloc after an unrelated Alloc")
        {
            uint64_t* a = arena.Alloc<uint64_t>();

            // Some random allocation in between
            uint64_t* otherAllocation = arena.Alloc<uint64_t>(2);

            uint64_t* b = arena.Realloc<uint64_t>(a, 1, 2);
            CHECK(a != b);
            CHECK(a != otherAllocation);
            CHECK(b != otherAllocation);
        }
    }

    SUBCASE("Free clears the arena")
    {
        arena.Free();

        CHECK(arena.data == nullptr);
        CHECK(arena.size == 0);
        CHECK(arena.offset == 0);
        CHECK(arena.next == nullptr);
    }

    arena.Free();
}
