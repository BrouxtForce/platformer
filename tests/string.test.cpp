#include "data-structures.hpp"
#include <doctest.h>

#include <string>
#include <array>
#include <random>

doctest::String toString(String str) {
    return doctest::String(str.data, str.size);
}

doctest::String toString(StringView str) {
    return doctest::String(str.data, str.size);
}

uint32_t GetRandom32()
{
    static std::mt19937 rng;
    return std::uniform_int_distribution<uint32_t>()(rng);
}

uint64_t GetRandom64()
{
    static std::mt19937_64 rng;
    return std::uniform_int_distribution<uint64_t>()(rng);
}

TEST_CASE("String")
{
    MemoryArena arena;
    arena.Init(64, MemoryArenaFlags_ClearToZero);

    String string;
    string.arena = &arena;

    SUBCASE("Equals")
    {
        string += 'a';
        CHECK(string != "b");
        CHECK(string != "aa");
        CHECK(string == "a");

        string += "c";
        CHECK(string != "a");
        CHECK(string != "ac  ");
        CHECK(string != "ad");
        CHECK(string == "ac");

        CHECK(arena.offset == 2);

        string.NullTerminate();
        CHECK(string == "ac");

        string.NullTerminate();
        CHECK(string == "ac");
    }

    SUBCASE("Appending")
    {
        string += "abc";
        string += "d";
        string += "efg";
        CHECK(string == "abcdefg");
        CHECK(arena.offset == 7);
    }

    SUBCASE("operator+()")
    {
        string += "abcd";
        CHECK(string + "efgh" == "abcdefgh");
        CHECK(string == "abcd");
        CHECK(arena.offset == 12);
    }

    SUBCASE("operator[]")
    {
        string += "abc";

        CHECK(string[0] == 'a');
        CHECK(string[1] == 'b');
        CHECK(string[2] == 'c');
    }

    SUBCASE("Append int16_t/uint16_t")
    {
        std::array<uint16_t, 32> nums { 0x0000, 0xffff, 0x8000, 0x7fff };
        for (size_t i = 4; i < nums.size(); i++)
        {
            nums[i] = (uint16_t)GetRandom32();
        }
        for (size_t i = 0; i < nums.size(); i++)
        {
            uint16_t a = nums[i];
            int16_t b = nums[i];

            string.Clear();
            string += a;
            CHECK(string == (StringView)std::to_string(a).c_str());

            string.Clear();
            string += b;
            CHECK(string == (StringView)std::to_string(b).c_str());
        }
    }

    SUBCASE("Append int32_t/uint32_t")
    {
        std::array<uint32_t, 32> nums { 0x0000'0000, 0xffff'ffff, 0x8000'0000, 0x7fff'ffff };
        for (size_t i = 4; i < nums.size(); i++)
        {
            nums[i] = GetRandom32();
        }
        for (size_t i = 0; i < nums.size(); i++)
        {
            uint32_t a = nums[i];
            int32_t b = nums[i];

            string.Clear();
            string += a;
            CHECK(string == (StringView)std::to_string(a).c_str());

            string.Clear();
            string += b;
            CHECK(string == (StringView)std::to_string(b).c_str());
        }
    }

    SUBCASE("Append int64_t/uint64_t")
    {
        std::array<uint64_t, 32> nums { 0x0000'0000'0000'0000, 0xffff'ffff'ffff'ffff, 0x8000'0000'0000'0000, 0x7fff'ffff'ffff'ffff };
        for (size_t i = 4; i < nums.size(); i++)
        {
            nums[i] = GetRandom64();
        }
        for (size_t i = 0; i < nums.size(); i++)
        {
            uint64_t a = nums[i];
            int64_t b = nums[i];

            string.Clear();
            string += a;
            CHECK(string == (StringView)std::to_string(a).c_str());

            string.Clear();
            string += b;
            CHECK(string == (StringView)std::to_string(b).c_str());
        }
    }
}

TEST_CASE("String View")
{
    SUBCASE("Equals")
    {
        StringView str = "abc";

        CHECK(str == "abc");
        CHECK(str != "ab");
        CHECK(str != "abcd");
        CHECK(str != "abd");
    }

    SUBCASE("constexpr string view")
    {
        constexpr StringView view = "argh";

        CHECK(view.size == 4);
        CHECK(view == "argh");
    }

    SUBCASE("EndsWith")
    {
        CHECK(StringView("abcdef").EndsWith(""));
        CHECK(StringView("abcdef").EndsWith("f"));
        CHECK(StringView("abcdef").EndsWith("ef"));
        CHECK(StringView("abcdef").EndsWith("def"));
        CHECK(StringView("abcdef").EndsWith("cdef"));
        CHECK(StringView("abcdef").EndsWith("abcdef"));
        CHECK_FALSE(StringView("abcdef").EndsWith("zabcdef"));
        CHECK_FALSE(StringView("abcdef").EndsWith("abcdefz"));
        CHECK_FALSE(StringView("abcdef").EndsWith("e"));
    }
}
