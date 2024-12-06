#include "camera.hpp"

Math::Matrix3x3 Camera::GetMatrix() const
{
    return Math::Matrix3x3(
        Math::float3( 1.0f / (transform.scale.x * aspect), 0,                        0 ),
        Math::float3( 0,                                   1.0f / transform.scale.y, 0 ),
        Math::float3(-transform.position.x,               -transform.position.y,     1 )
    );
}
