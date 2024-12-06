struct Material {
    color: vec3f,
};

@group(0) @binding(0)
var<uniform> material: Material;

@group(1) @binding(0)
var<uniform> modelMatrix: mat3x3f;

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

@vertex
fn quad_vert(@builtin(vertex_index) vid: u32) -> @builtin(position) vec4f {
    let pos = viewMatrix * modelMatrix * vec3f(positions[vid], 1);
    return vec4f(pos.xy, 0.0, 1.0);
}

@fragment
fn quad_frag() -> @location(0) vec4f {
    return vec4f(material.color, 1.0);
}
