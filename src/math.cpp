#include "math.hpp"

#include <cassert>

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

    float2 float2::operator-()
    {
        return float2(-x, -y);
    }

    float& float2::operator[](int index)
    {
        switch (index)
        {
            case 0: return x;
            case 1: return y;
        }
        assert(false);
        return x;
    }

    std::string float2::ToString()
    {
        return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }

    float2 operator+(const float2& left, const float2& right)
    {
        return float2(left.x + right.x, left.y + right.y);
    }

    float2 operator-(const float2& left, const float2& right)
    {
        return float2(left.x - right.x, left.y - right.y);
    }

    float2 operator*(const float2& left, const float2& right)
    {
        return float2(left.x * right.x, left.y * right.y);
    }

    float2 operator/(const float2& left, const float2& right)
    {
        return float2(left.x / right.x, left.y / right.y);
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

    float Length(float2 in)
    {
        return std::hypot(in.x, in.y);
    }

    float LengthSquared(float2 in)
    {
        return in.x * in.x + in.y * in.y;
    }

    float Distance(float2 a, float2 b)
    {
        return Length(a - b);
    }

    float DistanceSquared(float2 a, float2 b)
    {
        return LengthSquared(a - b);
    }

    float Dot(float2 a, float2 b)
    {
        return a.x * b.x + a.y * b.y;
    }

    float2 Normalize(float2 in)
    {
        float magnitude = std::hypot(in.x, in.y);
        return float2(in.x / magnitude, in.y / magnitude);
    }

    bool SolveQuadratic(float a, float b, float c, float2& result)
    {
        float discriminant = b*b - 4*a*c;
        if (discriminant < 0.0f)
        {
            return false;
        }
        float sqrtDiscriminant = std::sqrt(discriminant);
        float temp = 1.0f / (2.0f * a);
        result = float2(
            (-b + sqrtDiscriminant) * temp,
            (-b - sqrtDiscriminant) * temp
        );
        return true;
    }

    float LerpSmooth(float a, float b, float deltaTime, float halfLifeSeconds)
    {
        return b + (a - b) * std::exp2(-deltaTime / halfLifeSeconds);
    }

    float2 LerpSmooth(float2 a, float2 b, float deltaTime, float halfLifeSeconds)
    {
        return float2(
            LerpSmooth(a.x, b.x, deltaTime, halfLifeSeconds),
            LerpSmooth(a.y, b.y, deltaTime, halfLifeSeconds)
        );
    }

    float2 Direction(float angle)
    {
        return float2(std::cos(angle), std::sin(angle));
    }

    int DivideCeil(int dividend, int divisor)
    {
        return (dividend + divisor - 1) / divisor;
    }

    float2 Floor(float2 value)
    {
        return float2(std::floorf(value.x), std::floorf(value.y));
    }
}
