struct GlyphQuad
{
    position: vec2f,
    scale: vec2f,
    texPosition: vec2f,
    texScale: vec2f,
};

struct TextVertexOut
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

const QuadPositions: array<vec2f, 4> = array<vec2f, 4>(
    vec2f(-1, -1),
    vec2f( 1, -1),
    vec2f(-1,  1),
    vec2f( 1,  1)
);

@group(0) @binding(0) var<storage> glyphs: array<GlyphQuad>;
@group(0) @binding(1) var fontTexture: texture_2d<f32>;
@group(0) @binding(2) var fontSampler: sampler;

@vertex
fn text_vert(@builtin(vertex_index) vertexId: u32, @builtin(instance_index) instanceId: u32) -> TextVertexOut {
    var out: TextVertexOut;

    let position = QuadPositions[vertexId];
    let glyph = glyphs[instanceId];

    out.position = vec4f(position * glyph.scale + glyph.position, 0.0f, 1.0f);
    out.uv = (position + 1.0f) / 2.0f * glyph.texScale + glyph.texPosition;

    return out;
}

@fragment
fn text_frag(in: TextVertexOut) -> @location(0) vec4f {
    let distance = textureSample(fontTexture, fontSampler, in.uv).r;

    let cutoff = 180.0f / 255.0f;
    let smoothing = 1.0 / 32.0f;
    let alpha = smoothstep(cutoff - smoothing, cutoff + smoothing, distance);

    return vec4f(alpha);
}
