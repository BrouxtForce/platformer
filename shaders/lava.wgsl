struct Material
{
    color: vec3f,
};
@group(0) @binding(0) var<uniform> material: Material;

struct VertexOut
{
    @builtin(position) position: vec4f,
    @location(0) world_position: vec2f,
    @location(1) uv: vec2f,
};

@vertex
fn lava_vert(@builtin(vertex_index) vertex_id: u32) -> VertexOut {
    var out: VertexOut;

    let world_position = transform.model_matrix * vec3f(QuadPositions[vertex_id], 1);
    let view_position = view_matrix * world_position;

    out.world_position = world_position.xy;
    out.position = vec4f(view_position.xy, f32(transform.z_index) / U16_MAX, 1.0);
    out.uv = QuadPositions[vertex_id] * 0.5f + 0.5f;

    return out;
}

fn lava_height(x: f32) -> f32 {
    var sum = 0.0f;
    var frequency = 1.0f;
    var amplitude = 1.0f;
    var speed = 0.3f;
    for (var i = 0; i < 3; i++)
    {
        sum += amplitude * sin(frequency * x + speed * time);
        frequency += 0.5f;
        amplitude -= 0.1f;
        speed += 0.25f;
    }
    return 0.5f + sum * 0.1f;
}

@fragment
fn lava_frag(in: VertexOut) -> @location(0) vec4f {
    let position = vec2f(in.world_position.x, in.uv.y);
    if (lava_height(position.x) < position.y)
    {
        discard;
    }
    return vec4f(material.color, 1.0f);
}
