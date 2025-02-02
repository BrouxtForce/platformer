struct Material
{
    color: vec3f,
};
@group(0) @binding(0) var<uniform> material: Material;

struct VertexOut
{
    @builtin(position) position: vec4f,
    @location(0) model_pos: vec2f,
    @location(1) @interpolate(flat) scale: f32,
};

@vertex
fn radial_gravity_zone_vert(@builtin(vertex_index) vertex_id: u32) -> VertexOut {
    var out: VertexOut;

    let view_position: vec3f = view_matrix * transform.model_matrix * vec3f(QuadPositions[vertex_id], 1.0f);
    out.position = vec4f(view_position.xy, f32(transform.z_index) / U16_MAX, 1.0f);
    out.model_pos = QuadPositions[vertex_id];

    const arrow_size = 0.2f;
    let circumference = 2.0f * PI * length(transform.model_matrix[0]);
    let num_arrows = floor(circumference / arrow_size);
    out.scale = num_arrows / TAU;

    return out;
}

@fragment
fn radial_gravity_zone_frag(in: VertexOut) -> @location(0) vec4f {
    let squared_distance = dot(in.model_pos, in.model_pos);
    if (squared_distance > 1.0f)
    {
        discard;
    }
    let distance = sqrt(squared_distance) * in.scale;
    let scaled_angle = (atan2(in.model_pos.y, in.model_pos.x) + PI) * in.scale;

    let repeat_arrow_pos = fract(vec2f(scaled_angle, distance + time * 0.5f));
    let arrow = abs(-repeat_arrow_pos.x + 0.5f) < repeat_arrow_pos.y &&
                abs(-repeat_arrow_pos.x + 0.5f) > repeat_arrow_pos.y - 0.2f &&
                repeat_arrow_pos.x > 0.2f && repeat_arrow_pos.x < 0.8f;

    let strength = exp2(sin((distance * 0.5f + time) * 4.0f)) * 0.5f;
    var out_color = material.color;
    if (arrow)
    {
        out_color += vec3f(0.8f);
    }
    out_color *= strength;

    return vec4f(out_color, 1.0f);
}
