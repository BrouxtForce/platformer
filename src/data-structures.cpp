#include "data-structures.hpp"

#include <algorithm>

#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

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

void String::Append(unsigned short num)
{
    Append((unsigned int)num);
}

void String::Append(unsigned int num)
{
    size_t appendedSize = stbsp_snprintf(nullptr, 0, "%u", num);
    Reserve(size + appendedSize + 1);
    stbsp_snprintf(data + size, appendedSize + 1, "%u", num);
    size += appendedSize;
}

void String::Append(unsigned long num)
{
    size_t appendedSize = stbsp_snprintf(nullptr, 0, "%lu", num);
    Reserve(size + appendedSize + 1);
    stbsp_snprintf(data + size, appendedSize + 1, "%lu", num);
    size += appendedSize;
}

void String::Append(unsigned long long num)
{
    size_t appendedSize = stbsp_snprintf(nullptr, 0, "%llu", num);
    Reserve(size + appendedSize + 1);
    stbsp_snprintf(data + size, appendedSize + 1, "%llu", num);
    size += appendedSize;
}

void String::Append(short num)
{
    Append((int)num);
}

void String::Append(int num)
{
    size_t appendedSize = stbsp_snprintf(nullptr, 0, "%d", num);
    Reserve(size + appendedSize + 1);
    stbsp_snprintf(data + size, appendedSize + 1, "%d", num);
    size += appendedSize;
}

void String::Append(long num)
{
    size_t appendedSize = stbsp_snprintf(nullptr, 0, "%ld", num);
    Reserve(size + appendedSize + 1);
    stbsp_snprintf(data + size, appendedSize + 1, "%ld", num);
    size += appendedSize;
}

void String::Append(long long num)
{
    size_t appendedSize = stbsp_snprintf(nullptr, 0, "%lld", num);
    Reserve(size + appendedSize + 1);
    stbsp_snprintf(data + size, appendedSize + 1, "%lld", num);
    size += appendedSize;
}

void String::Append(double num)
{
    size_t appendedSize = stbsp_snprintf(nullptr, 0, "%f", num);
    Reserve(size + appendedSize + 1);
    stbsp_snprintf(data + size, appendedSize + 1, "%f", num);
    size += appendedSize;
}

void String::Clear()
{
    memset(data, 0, size);
    size = 0;
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

StringView StringView::Substr(size_t position, size_t length) const
{
    assert(position >= 0 && position <= size);

    StringView view;
    view.data = data + position;
    view.size = std::min(length, size - position);
    return view;
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

const char& StringView::operator[](size_t index) const
{
    assert(index >= 0 && index < size);
    return data[index];
}
