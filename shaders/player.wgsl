struct Material
{
    color: vec3f,
    eye_color: vec3f,

    inverse_left_eye_size: vec2f,
    left_eye_offset: vec2f,
    left_eyebrow_angle: f32,
    left_eyebrow_height: f32,

    inverse_right_eye_size: vec2f,
    right_eye_offset: vec2f,
    right_eyebrow_angle: f32,
    right_eyebrow_height: f32,
};
@group(0) @binding(0) var<uniform> material: Material;

struct VertexData
{
    @builtin(position) position: vec4f,
    @location(0) model_pos: vec2f,
    @location(1) left_eye_pos: vec2f,
    @location(2) left_eyebrow: f32,
    @location(3) right_eye_pos: vec2f,
    @location(4) right_eyebrow: f32,
};

@vertex
fn player_vert(@builtin(vertex_index) vertex_id: u32) -> VertexData {
    var out: VertexData;
    let pos = view_matrix * transform.model_matrix * vec3f(QuadPositions[vertex_id], 1);
    out.position = vec4f(pos.xy, f32(transform.z_index) / U16_MAX, 1.0);
    out.model_pos = QuadPositions[vertex_id];

    // Left eyebrow
    out.left_eye_pos = out.model_pos * material.inverse_left_eye_size - material.left_eye_offset;

    out.left_eyebrow =
        out.model_pos.x * sin(material.left_eyebrow_angle) +
        out.model_pos.y * cos(material.left_eyebrow_angle) - material.left_eyebrow_height;

    out.left_eyebrow -= material.left_eye_offset.y / material.inverse_left_eye_size.y;
    out.left_eyebrow -= material.left_eye_offset.x / material.inverse_left_eye_size.x * sin(material.left_eyebrow_angle);

    // Right eyebrow
    out.right_eye_pos = out.model_pos * material.inverse_right_eye_size - material.right_eye_offset;

    out.right_eyebrow =
        out.model_pos.x * sin(material.right_eyebrow_angle) +
        out.model_pos.y * cos(material.right_eyebrow_angle) - material.right_eyebrow_height;

    out.right_eyebrow -= material.right_eye_offset.y / material.inverse_right_eye_size.y;
    out.right_eyebrow -= material.right_eye_offset.x / material.inverse_right_eye_size.x * sin(material.right_eyebrow_angle);

    return out;
}

@fragment
fn player_frag(in: VertexData) -> @location(0) vec4f {
    if (dot(in.model_pos, in.model_pos) > 1.0)
    {
        discard;
    }

    if (dot(in.left_eye_pos, in.left_eye_pos) < 1.0f && in.left_eyebrow < 0.0f)
    {
        return vec4f(material.eye_color, 1.0f);
    }

    if (dot(in.right_eye_pos, in.right_eye_pos) < 1.0f && in.right_eyebrow < 0.0f)
    {
        return vec4f(material.eye_color, 1.0f);
    }

    return vec4f(material.color, 1.0f);
}
