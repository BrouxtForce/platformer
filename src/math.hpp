#pragma once

namespace Math
{
    struct float2
    {
        float x = 0.0f;
        float y = 0.0f;

        float2(float x, float y)
            : x(x), y(y) {}

        float2(float scalar)
            : x(scalar), y(scalar) {}

        float2 operator+(const float2& other);
        float2 operator-(const float2& other);
        float2 operator*(const float2& other);
        float2 operator/(const float2& other);
    };
}
