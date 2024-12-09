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

const U16_MAX = 0x1p16f - 1.0f;

@vertex
fn quad_vert(@builtin(vertex_index) vid: u32) -> @builtin(position) vec4f {
    let pos = viewMatrix * transform.modelMatrix * vec3f(positions[vid], 1);
    return vec4f(pos.xy, f32(transform.zIndex) / U16_MAX, 1.0);
}

@fragment
fn quad_frag() -> @location(0) vec4f {
    return vec4f(material.color, 1.0);
}
