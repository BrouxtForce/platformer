#include "data-structures.hpp"

#include <algorithm>

#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

#include "log.hpp"

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

void String::Append(Math::float2 vec)
{
    Append('(');
    Append(vec.x);
    Append(", ");
    Append(vec.y);
    Append(')');
}

void String::Append(Math::float3 vec)
{
    Append('(');
    Append(vec.x);
    Append(", ");
    Append(vec.y);
    Append(", ");
    Append(vec.z);
    Append(')');
}

void String::NullTerminate()
{
    if (capacity > size && data[size] == '\0')
    {
        return;
    }
    Append('\0');
    size--;
}

void String::ReplaceAll(char oldValue, char newValue)
{
    for (size_t i = 0; i < size; i++)
    {
        if (data[i] == oldValue)
        {
            data[i] = newValue;
        }
    }
}

void String::Clear()
{
    memset(data, 0, size);
    size = 0;
}

// TODO: Special handling in the case of null termination; The size of a string includes the null terminator
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

String String::Copy(StringView str, MemoryArena* arena)
{
    String out;
    out.arena = arena;
    out += str;
    return out;
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

bool StringView::StartsWith(StringView prefix) const
{
    if (prefix.size > size) return false;

    for (size_t i = 0; i < prefix.size; i++)
    {
        if (data[i] != prefix[i])
        {
            return false;
        }
    }
    return true;
}

bool StringView::EndsWith(StringView suffix) const
{
    if (suffix.size > size) return false;

    for (size_t i = 0; i < suffix.size; i++)
    {
        if (data[size - suffix.size + i] != suffix[i])
        {
            return false;
        }
    }
    return true;
}

size_t StringView::Find(char c, size_t start) const
{
    for (size_t i = start; i < size; i++)
    {
        if (data[i] == c)
        {
            return i;
        }
    }
    return String::NPOS;
}

// TODO: Use std::from_chars
int StringView::ToInt()
{
    constexpr size_t BUFFER_SIZE = 100;
    std::array<char, BUFFER_SIZE> buffer;
    assert(buffer.size() > size);

    memcpy(buffer.data(), data, size);
    buffer[size] = '\0';

    char* endptr = nullptr;
    int result = std::strtol(buffer.data(), &endptr, 10);

    // TODO: Is this ever not the case?
    static_assert(sizeof(long) >= sizeof(int));

    if (*endptr != '\0' || std::isspace(buffer[0]))
    {
        Log::Warn("ToInt('%') is invalid", *this);
    }
    return result;
}

// TODO: Use std::from_chars
float StringView::ToFloat()
{
    constexpr size_t BUFFER_SIZE = 100;
    std::array<char, BUFFER_SIZE> buffer;
    assert(buffer.size() > size);

    memcpy(buffer.data(), data, size);
    buffer[size] = '\0';

    char* endptr = nullptr;
    float result = std::strtof(buffer.data(), &endptr);

    if (*endptr != '\0' || std::isspace(buffer[0]))
    {
        Log::Warn("ToInt('%') is invalid", *this);
    }
    return result;
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
