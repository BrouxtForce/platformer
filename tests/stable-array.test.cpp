#include "data-structures.hpp"
#include <array>
#include <doctest.h>

TEST_CASE("Stable Array")
{
    MemoryArena arena;
    arena.Init(1024, MemoryArenaFlags_ClearToZero);

    StableArray<int> array;
    array.arena = &arena;

    SUBCASE("Filling an entire block")
    {
        std::array<int*, array.BlockSize> ptrs;
        for (int i = 0; i < ptrs.size(); i++)
        {
            ptrs[i] = array.Push(i + 1);
            CHECK(*ptrs[i] == i + 1);
        }
        for (int i = 0; i < ptrs.size(); i++)
        {
            CHECK(*ptrs[i] == i + 1);
            array.Erase(ptrs[i]);
        }
        for (int i = 0; i < ptrs.size(); i++)
        {
            ptrs[i] = array.Push(i + 1);
            CHECK(*ptrs[i] == i + 1);
            *ptrs[i] = i + 1;
        }
    }

    SUBCASE("Erase()")
    {
        std::array<int*, array.BlockSize> ptrs;
        for (int i = 0; i < ptrs.size(); i++)
        {
            ptrs[i] = array.Push(i + 1);
        }
        for (int i = 0; i < ptrs.size(); i += 2)
        {
            array.Erase(ptrs[i]);
            ptrs[i] = nullptr;
        }
        for (int i = 0; i < ptrs.size(); i += 2)
        {
            ptrs[i] = array.Push(i + 1);
        }
        for (int i = 0; i < ptrs.size(); i++)
        {
            CHECK(*ptrs[i] == i + 1);
        }
        CHECK(array.next->data == nullptr);
    }

    SUBCASE("Forward iterator")
    {
        std::array<int*, array.BlockSize * 64> ptrs;

        SUBCASE("Iterating and mutating the array")
        {
            for (int i = 0; i < ptrs.size(); i++)
            {
                ptrs[i] = array.Push(i + 1);
            }
            int index = 0;
            for (int& value : array)
            {
                CHECK(value == index + 1);
                value++;
                index++;
            }
            for (int i = 0; i < ptrs.size(); i++)
            {
                CHECK(*ptrs[i] == i + 2);
            }
        }

        SUBCASE("Iterating an empty array")
        {
            for (int& value : array)
            {
                CHECK(false);
            }
            for (int i = 0; i < ptrs.size(); i++)
            {
                ptrs[i] = array.Push(i + 1);
            }
            for (int i = 0; i < ptrs.size(); i++)
            {
                array.Erase(ptrs[i]);
            }
            for (int& value : array)
            {
                CHECK(false);
            }
        }

        SUBCASE("Iterating with completely empty blocks at the beginning")
        {
            for (int i = 0; i < ptrs.size(); i++)
            {
                ptrs[i] = array.Push(i + 1);
            }
            for (int i = 0; i < 256; i++)
            {
                array.Erase(ptrs[i]);
            }
            size_t index = 256;
            for (int& value : array)
            {
                CHECK(value == index + 1);
                index++;
            }
        }

        SUBCASE("Iterating with completely empty blocks in the middle")
        {
            for (int i = 0; i < ptrs.size(); i++)
            {
                ptrs[i] = array.Push(i + 1);
            }
            for (int i = 64; i < 256; i++)
            {
                array.Erase(ptrs[i]);
            }
            size_t index = 0;
            for (int& value : array)
            {
                CHECK(value == index + 1);
                index++;
                if (index == 64)
                {
                    index = 256;
                }
            }
        }

        SUBCASE("Iterating with completely empty blocks at the end")
        {
            for (int i = 0; i < ptrs.size(); i++)
            {
                ptrs[i] = array.Push(i + 1);
            }
            for (int i = 64; i < 256; i++)
            {
                array.Erase(ptrs[i]);
            }
            size_t index = 0;
            for (int& value : array)
            {
                CHECK(value == index + 1);
                index++;
                if (index == 64)
                {
                    index = 256;
                }
            }
        }

        SUBCASE("Iterating cheesy blocks")
        {
            for (int i = 0; i < ptrs.size(); i++)
            {
                ptrs[i] = array.Push(i + 1);
            }
            for (int i = 0; i < ptrs.size(); i += 2)
            {
                array.Erase(ptrs[i]);
            }
            size_t index = 1;
            for (int& value : array)
            {
                CHECK(value == index + 1);
                index += 2;
            }
        }
    }
}
