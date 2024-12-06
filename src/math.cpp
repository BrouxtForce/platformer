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

    float2& float2::operator+=(const float2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    float2& float2::operator-=(const float2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    float2& float2::operator*=(const float2& other)
    {
        x *= other.x;
        y *= other.y;
        return *this;
    }

    float2& float2::operator/=(const float2& other)
    {
        x /= other.x;
        y /= other.y;
        return *this;
    }

    float3 float3::operator+(const float3& other)
    {
        return float3(x + other.x, y + other.y, z + other.z);
    }

    float3 float3::operator-(const float3& other)
    {
        return float3(x - other.x, y - other.y, z - other.z);
    }

    float3 float3::operator*(const float3& other)
    {
        return float3(x * other.x, y * other.y, z * other.z);
    }

    float3 float3::operator/(const float3& other)
    {
        return float3(x / other.x, y / other.y, z / other.z);
    }

    Matrix3x3::Matrix3x3()
    {
        columns = {
            std::array<float, 4>{ 1, 0, 0 },
            std::array<float, 4>{ 0, 1, 0 },
            std::array<float, 4>{ 0, 0, 1 }
        };
    }

    Matrix3x3::Matrix3x3(float3 column0, float3 column1, float3 column2)
    {
        columns = {
            std::array<float, 4>{ column0.x, column0.y, column0.z },
            std::array<float, 4>{ column1.x, column1.y, column1.z },
            std::array<float, 4>{ column2.x, column2.y, column2.z }
        };
    }

    float2 Normalize(float2 in)
    {
        float magnitude = std::hypot(in.x, in.y);
        return float2(in.x / magnitude, in.y / magnitude);
    }
}
