struct Material {
    color: vec3f,
};

@group(0) @binding(0)
var<uniform> material: Material;

struct Transform {
    modelMatrix: mat3x3f,
    zIndex: u32,
};

@group(1) @binding(0)
var<uniform> transform: Transform;

@group(2) @binding(0)
var<uniform> viewMatrix: mat3x3f;

const positions: array<vec2f, 6> = array<vec2f, 6>(
    vec2f(-1, -1),
    vec2f( 1, -1),
    vec2f( 1,  1),
    vec2f( 1,  1),
    vec2f(-1,  1),
    vec2f(-1, -1)
);

struct VertexData
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

const U16_MAX = 0x1p16f - 1.0f;

@vertex
fn ellipse_vert(@builtin(vertex_index) vid: u32) -> VertexData {
    var out: VertexData;
    let pos = viewMatrix * transform.modelMatrix * vec3f(positions[vid], 1);
    out.position = vec4f(pos.xy, f32(transform.zIndex) / U16_MAX, 1.0);
    out.uv = positions[vid];
    return out;
}

@fragment
fn ellipse_frag(in: VertexData) -> @location(0) vec4f {
    if (dot(in.uv, in.uv) > 1.0)
    {
        discard;
    }
    return vec4f(material.color, 1.0);
}
