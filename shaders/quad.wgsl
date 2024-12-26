@vertex
fn quad_vert(@builtin(vertex_index) vid: u32) -> @builtin(position) vec4f {
    let pos = viewMatrix * transform.modelMatrix * vec3f(QuadPositions[vid], 1);
    return vec4f(pos.xy, f32(transform.zIndex) / U16_MAX, 1.0);
}

@fragment
fn quad_frag() -> @location(0) vec4f {
    return vec4f(material.color, 1.0);
}
