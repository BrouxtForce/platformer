#pragma once

namespace Math
{
    struct float2
    {
        float x = 0.0f;
        float y = 0.0f;

        float2() = default;

        inline float2(float x, float y)
            : x(x), y(y) {}

        inline float2(float scalar)
            : x(scalar), y(scalar) {}

        float2 operator+(const float2& other);
        float2 operator-(const float2& other);
        float2 operator*(const float2& other);
        float2 operator/(const float2& other);
    };

    struct Color
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;

        Color() = default;

        inline Color(float r, float g, float b, float a = 1.0f)
            : r(r), g(g), b(b), a(a) {}
    };
}
