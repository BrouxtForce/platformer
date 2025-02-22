#pragma once

#include <array>
#include <string>

namespace Math
{
    using int2 = std::array<int32_t, 2>;
    using int3 = std::array<int32_t, 3>;
    using int4 = std::array<int32_t, 4>;

    using uint2 = std::array<uint32_t, 2>;
    using uint3 = std::array<uint32_t, 3>;
    using uint4 = std::array<uint32_t, 4>;

    struct float2
    {
        float x = 0.0f;
        float y = 0.0f;

        float2() = default;

        inline float2(float x, float y)
            : x(x), y(y) {}

        inline float2(float scalar)
            : x(scalar), y(scalar) {}

        float2 operator+(const float2& other) const;
        float2 operator-(const float2& other) const;
        float2 operator*(const float2& other) const;
        float2 operator/(const float2& other) const;

        float2& operator+=(const float2& other);
        float2& operator-=(const float2& other);
        float2& operator*=(const float2& other);
        float2& operator/=(const float2& other);

        float2 operator+(const float& other) const;
        float2 operator-(const float& other) const;
        float2 operator*(const float& other) const;
        float2 operator/(const float& other) const;

        float2 operator-() const;

        float& operator[](int index);

        std::string ToString();
    };

    float2 operator+(const float& left, const float2& right);
    float2 operator-(const float& left, const float2& right);
    float2 operator*(const float& left, const float2& right);
    float2 operator/(const float& left, const float2& right);

    struct float3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        float3() = default;

        inline float3(float x, float y, float z)
            : x(x), y(y), z(z) {}

        inline float3(float scalar)
            : x(scalar), y(scalar), z(scalar) {}

        float3 operator+(const float3& other) const;
        float3 operator-(const float3& other) const;
        float3 operator*(const float3& other) const;
        float3 operator/(const float3& other) const;
    };

    using float4 = std::array<float, 4>;

    struct Complex
    {
        float a = 0.0f;
        float b = 0.0f;

        Complex(float a, float b);

        static Complex FromAngle(float rotation);

        float2 operator*(const float2& other) const;
    };

    float2 operator*(const float2& left, const Complex& right);

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

    struct Matrix3x3
    {
        // Padded to 3 float4's to directly upload to WebGPU
        std::array<std::array<float, 4>, 3> columns;

        // Constucts the identity matrix
        Matrix3x3();

        Matrix3x3(float3 column0, float3 column1, float3 column2);
    };

    float Length(float2 in);
    float LengthSquared(float2 in);
    float Distance(float2 a, float2 b);
    float DistanceSquared(float2 a, float2 b);
    float Dot(float2 a, float2 b);
    float2 Normalize(float2 in);

    // Returns true if successful
    bool SolveQuadratic(float a, float b, float c, float2& result);

    float LerpSmooth(float a, float b, float deltaTime, float halfLifeSeconds);
    float2 LerpSmooth(float2 a, float2 b, float deltaTime, float halfLifeSeconds);
    float LerpAngleSmooth(float alpha, float beta, float deltaTime, float halfLifeSeconds);

    float2 Direction(float angle);

    float2 RotateVector(float2 vector, float angle);

    // Computes ceil(dividend / divisor)
    int DivideCeil(int dividend, int divisor);

    float2 Floor(float2 value);

    template<typename T0, typename T1>
    requires (std::is_same_v<T0, T1>)
    inline T0 Max(T0 a, T1 b)
    {
        if (a > b) return a;
        return b;
    }

    template<typename T0, typename T1>
    requires (std::is_same_v<T0, T1>)
    inline T0 Min(T0 a, T1 b)
    {
        if (a < b) return a;
        return b;
    }

    constexpr double PI = M_PI;
    constexpr double RAD_TO_DEG = 180.0 / PI;
    constexpr double DEG_TO_RAD = PI / 180.0;
    constexpr double SQRT_2 = M_SQRT2;
}
