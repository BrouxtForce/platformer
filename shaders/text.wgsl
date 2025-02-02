struct Material
{
    color: vec3f,
};
@group(0) @binding(0) var<uniform> material: Material;

struct GlyphQuad
{
    position: vec2f,
    scale: vec2f,
    tex_position: vec2f,
    tex_scale: vec2f,
};

struct TextVertexOut
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@group(3) @binding(0) var<storage> glyphs: array<GlyphQuad>;
@group(3) @binding(1) var fontTexture: texture_2d<f32>;
@group(3) @binding(2) var fontSampler: sampler;

@vertex
fn text_vert(@builtin(vertex_index) vertex_id: u32, @builtin(instance_index) instance_id: u32) -> TextVertexOut {
    var out: TextVertexOut;

    let glyph = glyphs[instance_id];
    let position = QuadPositions[vertex_id] * glyph.scale + glyph.position;

    out.position = vec4f(view_matrix * transform.model_matrix * vec3f(position, 1), 1);
    out.uv = (QuadPositions[vertex_id] + 1.0f) / 2.0f * glyph.tex_scale + glyph.tex_position;

    out.position.z = f32(transform.z_index) / U16_MAX;

    return out;
}

@fragment
fn text_frag(in: TextVertexOut) -> @location(0) vec4f {
    let distance = textureSample(fontTexture, fontSampler, in.uv).r;

    let cutoff = 180.0f / 255.0f;
    let smoothing = 1.0 / 32.0f;
    let alpha = smoothstep(cutoff - smoothing, cutoff + smoothing, distance);

    return vec4f(material.color * alpha, alpha);
}
