@group(0) @binding(0) var input_texture: texture_2d<f32>;
@group(0) @binding(1) var input_sampler: sampler;

struct VertexOut
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) @interpolate(flat) jump: u32,
};

@vertex
fn jump_flood_vert(@builtin(vertex_index) vertex_id: u32, @builtin(instance_index) instance_id: u32) -> VertexOut {
    var out: VertexOut;

    let position = QuadPositions[vertex_id];
    out.position = vec4f(QuadPositions[vertex_id], 0, 1);
    out.uv = (position + 1.0f) / 2.0f;
    out.uv.y = 1.0f - out.uv.y;

    // The jump is passed to the shader via the first instance id
    out.jump = instance_id;

    return out;
}

fn is_within_bounds(coords: vec2i, texture_size: vec2u) -> bool {
    return all(coords > vec2i(0)) && all(coords < vec2i(texture_size));
}

@fragment
fn jump_flood_init_frag(in: VertexOut) -> @location(0) vec2f {
    let sample: vec4f = textureSample(input_texture, input_sampler, in.uv);
    if (sample.a > 0.5f)
    {
        return in.uv;
    }
    return vec2f(-1.0f);
}

fn compute_jump_flood(in: VertexOut) -> vec2f {
    var pixel_coords = vec2i(in.position.xy);

    let texture_size: vec2u = textureDimensions(input_texture);

    var min_distance: f32 = 100.0f;
    var closest_uv: vec2f;
    var offset: vec2i;
    for (offset.x = -1; offset.x <=1; offset.x++)
    {
        for (offset.y = -1; offset.y <= 1; offset.y++)
        {
            let sample_coords: vec2i = pixel_coords + offset * i32(in.jump);
            if (!is_within_bounds(sample_coords, texture_size))
            {
                continue;
            }

            let sample_uv: vec2f = textureLoad(input_texture, sample_coords, 0).rg;

            let distance = length(in.uv - sample_uv);
            if (distance < min_distance)
            {
                min_distance = distance;
                closest_uv = sample_uv;
            }
        }
    }

    return closest_uv;
}

@fragment
fn jump_flood_main_frag(in: VertexOut) -> @location(0) vec2f {
    return compute_jump_flood(in);
}

@fragment
fn jump_flood_final_pass_frag(in: VertexOut) -> @location(0) vec2f {
    let closest_uv = compute_jump_flood(in);
    return vec2f(distance(closest_uv, in.uv), 0.0f);
}
