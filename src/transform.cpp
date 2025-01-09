#include "transform.hpp"

Math::Matrix3x3 Transform::GetMatrix() const
{
    Math::float2 direction = Math::Direction(rotation);
    return Math::Matrix3x3(
        Math::float3( scale.x *  direction.x, scale.x * direction.y, 0 ),
        Math::float3( scale.y * -direction.y, scale.y * direction.x, 0 ),
        Math::float3( position.x,             position.y,            1 )
    );
}
