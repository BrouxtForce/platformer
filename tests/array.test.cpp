#include "data-structures.hpp"
#include <doctest.h>

TEST_CASE("Array")
{
    MemoryArena arena;
    arena.Init(64, MemoryArenaFlags_ClearToZero);

    Array<int> array;
    array.arena = &arena;

    SUBCASE("Empty slots after Resize() should be zeroed")
    {
        for (int i = 0; i < 8; i++)
        {
            array.Push(i + 1);
        }

        array.Resize(16);

        for (int k = 0; k < 2; k++)
        {
            for (int i = 0; i < 16; i++)
            {
                if (i < 8)
                {
                    CHECK(array[i] == i + 1);
                }
                else
                {
                    CHECK(array[i] == 0);
                    array[i] = i + 1;
                }
            }

            array.Resize(32);
            array.Resize(8);
            array.Resize(16);
        }
    }

    SUBCASE("Empty slots after Pop() should be zeroed after Resize()")
    {
        for (int i = 0; i < 8; i++)
        {
            array.Push(i + 1);
        }
        for (int i = 0; i < 8; i++)
        {
            CHECK(array.size == 8 - i);
            array.Pop();
        }
        array.Resize(16);
        for (int i = 0; i < 8; i++)
        {
            CHECK(array[i] == 0);
        }
    }

    SUBCASE("Resize() reallocates in place if nothing else was allocated")
    {
        array.Reserve(8);
        int* prevData = array.data;
        array.Resize(16);
        CHECK(array.data == prevData);
    }

    SUBCASE("Reserve() reallocates in place if nothing else was allocated")
    {
        array.Reserve(8);
        int* prevData = array.data;
        array.Reserve(16);
        CHECK(array.data == prevData);
    }

    SUBCASE("Array iterator")
    {
        for (int i = 0; i < 16; i++)
        {
            array.Push(i + 1);
        }
        for (int& num : array)
        {
            num--;
        }
        for (int i = 0; i < 16; i++)
        {
            CHECK(array[i] == i);
        }
    }

    arena.Free();
}

TEST_CASE("Span")
{
    MemoryArena arena;
    arena.Init(128, MemoryArenaFlags_ClearToZero);

    Array<int> array;
    array.arena = &arena;

    for (int i = 1; i <= 32; i++)
    {
        array.Push(i);
    }

    SUBCASE("Iterator")
    {
        Span span = array;
        int index = 0;
        for (int& num : span)
        {
            num--;
        }

        for (int i = 0; i < 32; i++)
        {
            CHECK(array[i] == i);
        }
    }

    SUBCASE("operator[]")
    {
        Span span = array;
        for (int i = 0; i < span.size; i++)
        {
            CHECK(span[i] == i + 1);
        }
    }

    SUBCASE("Slice()")
    {
        Span span = array;
        CHECK(span == array);

        span = span.Slice(0);
        CHECK(span == array);

        span = span.Slice(1);
        CHECK(span.size == array.size - 1);
        for (int i = 1; i < array.size; i++)
        {
            CHECK(span[i - 1] == array[i]);
        }

        span = span.Slice(1, 1);
        CHECK(span[0] == 3);
        CHECK(span.size == 1);
    }
}
