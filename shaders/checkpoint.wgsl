struct VertexOut
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn checkpoint_vert(@builtin(vertex_index) vid: u32) -> VertexOut
{
    var out: VertexOut;

    let position = viewMatrix * transform.modelMatrix * vec3f(QuadPositions[vid], 1);
    out.position = vec4f(position.xy, f32(transform.zIndex) / U16_MAX, 1.0);

    let aspect = transform.modelMatrix[0][0] / transform.modelMatrix[1][1];
    out.uv = QuadPositions[vid] * vec2f(aspect, 1.0f) * 0.5f + 0.5f;

    return out;
}

@fragment
fn checkpoint_frag(in: VertexOut) -> @location(0) vec4f {
    var v = 0.1f * mix(sin(in.uv.x + time * 2.0f), 0.0f, in.uv.x / 0.65f);

    var flag: bool = (in.uv.y >= 0.5f + v && in.uv.y <= 0.8f + v && in.uv.x >= 0.25f && in.uv.x <= 0.65f) ||
                     (in.uv.x >= 0.65f && in.uv.x <= 0.75f && in.uv.y >= 0.2f && in.uv.y <= 0.8f);

    var color: vec3f = material.color;
    if (material.flags != 0)
    {
        let strength = abs(4.0f * (time - material.value_a) - length(in.uv * 2.0f - 1.0f) - 1.0f);
        color += vec3f(0.5f) * clamp(sin(strength) / strength, 0.0f, 1.0f);
        if (flag)
        {
            color = vec3f(min(2.0f * (time - material.value_a), 1.0f));
        }
    }
    else if (flag)
    {
        color -= 0.5f;
    }

    return vec4f(color, 0.0f);
}
