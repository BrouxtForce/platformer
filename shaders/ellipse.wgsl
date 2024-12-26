struct VertexData
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn ellipse_vert(@builtin(vertex_index) vid: u32) -> VertexData {
    var out: VertexData;
    let pos = viewMatrix * transform.modelMatrix * vec3f(QuadPositions[vid], 1);
    out.position = vec4f(pos.xy, f32(transform.zIndex) / U16_MAX, 1.0);
    out.uv = QuadPositions[vid];
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
