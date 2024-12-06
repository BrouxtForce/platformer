#include "transform.hpp"

Math::Matrix3x3 Transform::GetMatrix() const
{
    return Math::Matrix3x3(
        Math::float3( scale.x,    0,          0 ),
        Math::float3( 0,          scale.y,    0 ),
        Math::float3( position.x, position.y, 1 )
    );
}
