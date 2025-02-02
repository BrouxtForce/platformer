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
};

@vertex
fn checkpoint_vert(@builtin(vertex_index) vertex_id: u32) -> VertexOut
{
    var out: VertexOut;

    let position = view_matrix * transform.model_matrix * vec3f(QuadPositions[vertex_id], 1);
    out.position = vec4f(position.xy, f32(transform.z_index) / U16_MAX, 1.0);

    // Start with range vec2f([-aspect, aspect], [-1, 1])
    let aspect = transform.model_matrix[0][0] / transform.model_matrix[1][1];
    out.uv = QuadPositions[vertex_id] * vec2f(aspect, 1.0f);

    var theta = 0.1f;
    if (bool(material.started))
    {
        let x = 4.0f * (time - material.start_time);
        theta += 0.5f * exp(-0.25f * x) * sin(x);
    }

    // Rotate the uv
    let cos_theta = cos(theta);
    let sin_theta = sin(theta);

    out.uv = vec2f(
        out.uv.x * cos_theta - out.uv.y * sin_theta,
        out.uv.x * sin_theta + out.uv.y * cos_theta
    );

    // Convert to pseudo-[0, 1] range
    out.uv = out.uv * 0.5f + 0.5f;

    return out;
}

@fragment
fn checkpoint_frag(in: VertexOut) -> @location(0) vec4f {
    var v = 0.1f * mix(sin(4.0f * in.uv.x + time * 2.0f), 0.0f, in.uv.x / 0.65f);

    var flag: bool = (in.uv.y >= 0.5f + v && in.uv.y <= 0.8f + v && in.uv.x >= 0.25f && in.uv.x <= 0.65f) ||
                     (in.uv.x >= 0.65f && in.uv.x <= 0.75f && in.uv.y >= 0.2f && in.uv.y <= 0.8f);

    var color: vec3f = material.color;
    if (bool(material.started))
    {
        let strength = abs(4.0f * (time - material.start_time) - length(in.uv * 2.0f - 1.0f) - 1.0f);
        color += vec3f(0.5f) * clamp(sin(strength) / strength, 0.0f, 1.0f);
        if (flag)
        {
            color = vec3f(min(2.0f * (time - material.start_time), 1.0f));
        }
    }
    else if (flag)
    {
        color -= 0.5f;
    }

    return vec4f(color, 0.0f);
}
