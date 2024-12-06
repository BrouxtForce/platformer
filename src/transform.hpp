#pragma once

#include "math.hpp"

struct Transform
{
    Math::float2 position = 0.0f;
    Math::float2 scale = 1.0f;

    Transform() = default;
    inline Transform(Math::float2 position, Math::float2 scale)
        : position(position), scale(scale) {}

    Math::Matrix3x3 GetMatrix() const;
};
