#include "math.hpp"

namespace Math
{
    float2 float2::operator+(const float2& other)
    {
        return float2(x + other.x, y + other.y);
    }

    float2 float2::operator-(const float2& other)
    {
        return float2(x - other.x, y - other.y);
    }

    float2 float2::operator*(const float2& other)
    {
        return float2(x * other.x, y * other.y);
    }

    float2 float2::operator/(const float2& other)
    {
        return float2(x / other.x, y / other.y);
    }
}
