struct VertexOut
{
    @builtin(position) position: vec4f,
    @location(0) scaled_model_pos_y: f32,
    @location(1) @interpolate(flat) num_arrows: f32,
    @location(2) arrow_pos: vec2f,
};

@vertex
fn gravity_zone_vert(@builtin(vertex_index) vid: u32) -> VertexOut {
    var out: VertexOut;

    let view_position = viewMatrix * transform.modelMatrix * vec3f(QuadPositions[vid], 1);
    out.position = vec4f(view_position.xy, f32(transform.zIndex) / U16_MAX, 1.0);

    let scale = vec2f(length(transform.modelMatrix[0]), length(transform.modelMatrix[1]));
    out.scaled_model_pos_y = scale.y * QuadPositions[vid].y;

    const arrow_size = 0.2f;
    out.num_arrows = floor(scale.x * 2.0f / arrow_size);

    let padding_x = 1.0f - out.num_arrows * arrow_size / (scale.x * 2.0f);

    out.arrow_pos = (QuadPositions[vid] / vec2f(1.0f - padding_x) * 0.5f + 0.5f) * out.num_arrows;
    out.arrow_pos.y *= scale.y / scale.x;
    out.arrow_pos.y += time;

    return out;
}

@fragment
fn gravity_zone_frag(in: VertexOut) -> @location(0) vec4f {
    let repeat_arrow_pos = fract(in.arrow_pos);
    let arrow = abs(-repeat_arrow_pos.x + 0.5f) < repeat_arrow_pos.y &&
                abs(-repeat_arrow_pos.x + 0.5f) > repeat_arrow_pos.y - 0.2f &&
                repeat_arrow_pos.x > 0.2f && repeat_arrow_pos.x < 0.8f &&
                in.arrow_pos.x > 0.0f && in.arrow_pos.x < in.num_arrows;

    let strength = exp2(sin((in.scaled_model_pos_y + time) * 4.0f)) * 0.5f;
    var out_color = material.color;
    if (arrow)
    {
        out_color += vec3f(0.8f);
    }
    out_color *= strength;

    return vec4f(out_color, 1.0f);
}
