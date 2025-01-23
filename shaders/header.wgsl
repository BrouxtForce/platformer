struct Material {
    color: vec3f,
    color_alpha: f32,
    flags: u32,
    value_a: f32,
    value_b: f32,
    value_c: f32,
};

@group(0) @binding(0)
var<uniform> material: Material;

struct Transform {
    model_matrix: mat3x3f,
    z_index: u32,
};

@group(1) @binding(0)
var<uniform> transform: Transform;

@group(2) @binding(0)
var<uniform> view_matrix: mat3x3f;
@group(2) @binding(1)
var<uniform> time: f32;

const QuadPositions: array<vec2f, 4> = array<vec2f, 4>(
    vec2f(-1, -1),
    vec2f( 1, -1),
    vec2f(-1,  1),
    vec2f( 1,  1)
);

const U16_MAX = 0x1p16f - 1.0f;
const PI  = 3.14159265359f;
const TAU = 6.28318530718f;
