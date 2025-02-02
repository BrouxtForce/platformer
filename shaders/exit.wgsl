struct Material
{
    color: vec3f,
    started: u32,
    start_time: f32,
};
@group(0) @binding(0)
var<uniform> material: Material;

struct VertexOut
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) @interpolate(flat) open_t: f32,
    @location(2) @interpolate(flat) rotation_theta: f32,
};

fn ease(x: f32) -> f32 {
    return -(cos(PI * x) - 1.0f) / 2.0f;
}

@vertex
fn exit_vert(@builtin(vertex_index) vertex_id: u32) -> VertexOut {
    var out: VertexOut;

    let view_position = view_matrix * transform.model_matrix * vec3f(QuadPositions[vertex_id], 1);
    out.position = vec4f(view_position.xy, f32(transform.z_index) / U16_MAX, 1.0f);
    out.uv = QuadPositions[vertex_id] * 0.5f + 0.5f;

    if (!bool(material.started))
    {
        out.open_t = 0.0f;
        out.rotation_theta = 0.0f;
    }
    else
    {
        let time_since_open = time - material.start_time;
        if (time_since_open < 2.0f)
        {
            out.open_t = min(time_since_open, 0.5f) * 0.125f;
            out.rotation_theta = ease(0.5f * time_since_open) * PI;
        }
        else
        {
            out.rotation_theta = 0.0f;

            let ease_t = 0.25f * (time_since_open - 2.0f);
            if (ease_t < 1.0f)
            {
                out.open_t = 0.0625f + 0.9375f * ease(ease_t);
            }
            else
            {
                out.open_t = 1.0f;
            }
        }
    }

    return out;
}

fn rounded_x_sdf(position: vec2f, width: f32, radius: f32) -> f32 {
    let abs_position = abs(position);
    return length(abs_position - 0.5f * min(abs_position.x + abs_position.y, width)) - radius;
}

@fragment
fn exit_frag(in: VertexOut) -> @location(0) vec4f {
    let quadrant = u32(2.0f * floor(in.uv.x * 2.0f) + floor(in.uv.y * 2.0f));

    var quadrant_uv = fract(in.uv * 2.0f);
    switch (quadrant)
    {
        case 0:  { quadrant_uv = vec2f( quadrant_uv.y, 1.0f - quadrant_uv.x); }
        case 1:  { quadrant_uv = vec2f( quadrant_uv.x,  quadrant_uv.y); }
        case 2:  { quadrant_uv = vec2f( 1.0f - quadrant_uv.x,  1.0f - quadrant_uv.y); }
        case 3:  { quadrant_uv = vec2f( 1.0f - quadrant_uv.y,  quadrant_uv.x); }
        default: { return vec4f(1.0f, 0.0f, 1.0f, 1.0f); }
    }

    if (quadrant_uv.y < in.open_t)
    {
        return vec4f(0, 0, 0, 0);
    }
    quadrant_uv.y -= in.open_t;

    const border_size = 0.05f;
    let border: bool = any(quadrant_uv < vec2f(border_size)) || any(quadrant_uv > vec2f(1.0f - border_size));
    if (border)
    {
        return vec4f(material.color - 0.1f - 0.05f * f32(quadrant == 1 || quadrant == 2), 1.0f);
    }

    let cos_theta = cos(in.rotation_theta);
    let sin_theta = sin(in.rotation_theta);
    var rotated_uv = quadrant_uv * 2.0f - 1.0f;
    rotated_uv = vec2f(
        rotated_uv.x * cos_theta - rotated_uv.y * sin_theta,
        rotated_uv.x * sin_theta + rotated_uv.y * cos_theta
    );
    let handle: bool = rounded_x_sdf(rotated_uv, 0.4f, 0.1f) < 0.0f;
    if (handle)
    {
        return vec4f(material.color - 0.2f, 1.0f);
    }

    return vec4f(material.color, 1.0f);
}
