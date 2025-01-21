@vertex
fn quad_vert(@builtin(vertex_index) vertex_id: u32) -> @builtin(position) vec4f {
    let pos = view_matrix * transform.model_matrix * vec3f(QuadPositions[vertex_id], 1);
    return vec4f(pos.xy, f32(transform.z_index) / U16_MAX, 1.0);
}

@fragment
fn quad_frag() -> @location(0) vec4f {
    return vec4f(material.color, 1.0);
}
