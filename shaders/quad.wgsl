struct Material {
    color: vec3f,
};

@group(0) @binding(0)
var<uniform> material: Material;

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
    let pos = positions[vid] * 0.5;
    return vec4f(pos, 0.0, 1.0);
}

@fragment
fn quad_frag() -> @location(0) vec4f {
    return vec4f(material.color, 1.0);
}
