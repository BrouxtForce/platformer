#include "data-structures.hpp"

void String::Append(StringView str)
{
    Reserve(size + str.size);
    memcpy(data + size, str.data, str.size);
    size += str.size;
}

void String::Append(char c)
{
    Push(c);
}

bool String::Equals(StringView str) const
{
    if (size != str.size)
    {
        return false;
    }
    return memcmp(data, str.data, size) == 0;
}

String String::operator+(StringView other) const
{
    String str;
    str.arena = arena;
    str.Reserve(size + other.size);
    str.Append(*this);
    str.Append(other);

    return str;
}

String String::operator+(char c) const
{
    String str;
    str.arena = arena;
    str.Reserve(size + 1);
    str.Append(*this);
    str.Append(c);

    return str;
}

void String::operator+=(StringView other)
{
    Append(other);
}

void String::operator+=(char c)
{
    Append(c);
}

bool String::operator==(StringView str) const
{
    return Equals(str);
}

bool String::operator!=(StringView str) const
{
    return !Equals(str);
}

StringView::StringView(const String& str)
{
    data = str.data;
    size = str.size;
}

StringView::StringView(const char* str)
{
    data = str;
    size = strlen(str);
}

bool StringView::Equals(StringView str) const
{
    if (size != str.size)
    {
        return false;
    }
    return memcmp(data, str.data, size) == 0;
}

bool StringView::operator==(StringView str) const
{
    return Equals(str);
}

bool StringView::operator!=(StringView str) const
{
    return !Equals(str);
}
