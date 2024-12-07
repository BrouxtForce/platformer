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

struct VertexData
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn ellipse_vert(@builtin(vertex_index) vid: u32) -> VertexData {
    var out: VertexData;
    let pos = viewMatrix * modelMatrix * vec3f(positions[vid], 1);
    out.position = vec4f(pos.xy, 0.0, 1.0);
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
