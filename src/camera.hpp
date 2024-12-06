#pragma once

#include "transform.hpp"

class Camera
{
public:
    Transform transform;
    float aspect = 1.0f;

    Math::Matrix3x3 GetMatrix() const;
};