#include "data-structures.hpp"
#include <doctest.h>

doctest::String toString(String str) {
    return doctest::String(str.data, str.size);
}

doctest::String toString(StringView str) {
    return doctest::String(str.data, str.size);
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
}
