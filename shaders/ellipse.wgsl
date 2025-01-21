struct VertexData
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn ellipse_vert(@builtin(vertex_index) vertex_id: u32) -> VertexData {
    var out: VertexData;
    let pos = view_matrix * transform.model_matrix * vec3f(QuadPositions[vertex_id], 1);
    out.position = vec4f(pos.xy, f32(transform.z_index) / U16_MAX, 1.0);
    out.uv = QuadPositions[vertex_id];
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
