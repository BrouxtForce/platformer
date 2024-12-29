struct CascadeUniforms
{
    // The square root of the number of rays in a probe (pixels of memory the probe will use)
    probe_size: u32,

    // The offset from the center of the probe that the raymarching will start sampling from
    interval_start: f32,

    // The length of each ray in a probe
    interval_length: f32,

    // The number of cascades in total. This value is uniform with the rest of the array
    cascade_count: u32,

    // The number of pixels in the cascade. This value is uniform with the rest of the array
    texture_size: vec2u,
};

@group(0) @binding(0) var<storage> cascade_uniforms_array: array<CascadeUniforms>;

@group(1) @binding(0) var radiance_texture: texture_2d<f32>;
@group(1) @binding(1) var radiance_sampler: sampler;

@group(2) @binding(0) var cascade_texture: texture_2d<f32>;

struct VertexData
{
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) @interpolate(flat) cascade_index: u32,
};

@vertex
fn fullscreen_quad_vert(@builtin(vertex_index) vertex_id: u32, @builtin(instance_index) instance_id: u32) -> VertexData {
    var out: VertexData;

    let position = QuadPositions[vertex_id];
    out.position = vec4f(position, 0, 1);
    out.uv = (position + 1.0f) / 2.0f;
    out.uv.y = 1.0f - out.uv.y;

    // Important! The cascade index is passed to the shader by setting the first instance index to the cascade index
    // This is kind of lazy, but is super convenient for just passing a single uniform value to the shader
    // This will be used in the fragment shader
    out.cascade_index = instance_id;

    return out;
}

@fragment
fn radiance_frag() -> @location(0) vec4f {
    return vec4f(0, 0, 0, 1);
}

fn circle_sdf(circle_position: vec2f, circle_radius: f32, ray_position: vec2f) -> f32 {
    return length(ray_position - circle_position) - circle_radius;
}

fn scene_sdf(ray_position: vec2f) -> f32 {
    return circle_sdf(vec2f(0.5, 0.5), 0.1, ray_position);
}

/*
 * Raymarches into the scene. The return value is the color of the ray, with an alpha
 * value of 1.0f if there was a collision and 0.0f otherwise.
*/
fn raymarch_scene(ray_position: vec2f, ray_direction: vec2f, ray_length: f32) -> vec4f {
    const MAX_ITERATIONS = 10;
    const HIT_DISTANCE = 0.001f;

    var distance_travelled = 0.0f;
    var hit = false;
    for (var i = 0; i < MAX_ITERATIONS; i++)
    {
        let distance = scene_sdf(ray_position + ray_direction * distance_travelled);
        distance_travelled += distance;

        if (distance < HIT_DISTANCE)
        {
            hit = true;
            break;
        }
        if (distance_travelled > ray_length)
        {
            break;
        }
    }

    // TODO: Texture sample the radiance texture
    let radiance = vec3f(1.0f);
    if (hit)
    {
        return vec4f(radiance, 0.0f);
    }
    return vec4f(0.0f, 0.0f, 0.0f, 1.0f);
}

/*
 * Calculates value of the current pixel in the current cascade, unmerged
*/
fn get_pixel_radiance(in: VertexData) -> vec4f {
    let pixel_coords = vec2u(in.position.xy);
    let cascade_uniforms = cascade_uniforms_array[in.cascade_index];

    // I use the term "block" here to refer to a grouping of rays facing the same direction.
    // block_size will be proportional to the texture size
    let block_size    : vec2u = cascade_uniforms.texture_size / cascade_uniforms.probe_size;
    let block_index_2d: vec2u = pixel_coords / block_size;
    let block_index_1d: u32   = block_index_2d.y * cascade_uniforms.probe_size + block_index_2d.x;
    let block_pixel   : vec2u = pixel_coords - block_index_2d * block_size;
    let block_count   : u32   = cascade_uniforms.probe_size * cascade_uniforms.probe_size;

    let ray_position : vec2f = vec2f(block_pixel) / vec2f(block_size);
    let ray_angle    : f32   = TAU * (f32(block_index_1d) + 0.5f) / f32(block_count);
    let ray_direction: vec2f = vec2f(cos(ray_angle), sin(ray_angle));

    return raymarch_scene(
        ray_position + ray_direction * cascade_uniforms.interval_start,
        ray_direction,
        cascade_uniforms.interval_length
    );
}

/*
 * Merges two intervals with respect to the alpha values
 * If the near interval's alpha is 0, then far interval will not be merged
*/
fn merge_intervals(near_interval: vec4f, far_interval: vec4f) -> vec4f {
    let radiance = near_interval.rgb + (far_interval.rgb * near_interval.a);
    return vec4f(radiance, near_interval.a * far_interval.a);
}

/*
 * Computes both the pixel value of the current cascade (using get_pixel_radiance()) and
 * merges that value with the upper cascade (if there is one)
*/
@fragment
fn merge_cascades_frag(in: VertexData) -> @location(0) vec4f {
    let current_interval = get_pixel_radiance(in);

    // Early out if this is the top cascade layer
    if (in.cascade_index + 1 >= cascade_uniforms_array[in.cascade_index].cascade_count)
    {
        return current_interval;
    }

    let pixel_coords = vec2u(in.position.xy);

    var uv: vec2f;
    var direction_index: u32;

    // Calculate uv and direction_index on the current cascade layer
    {
        let cascade_uniforms = cascade_uniforms_array[in.cascade_index];
        let block_size : vec2u = cascade_uniforms.texture_size / cascade_uniforms.probe_size;
        let block_index: vec2u = pixel_coords / block_size;
        let block_pixel: vec2u = pixel_coords - block_index * block_size;

        uv = vec2f(block_pixel) / vec2f(block_size);
        direction_index = (block_index.y * cascade_uniforms.probe_size + block_index.x) * 4;
    }

    // Merge the upper cascade onto this cascade
    {
        let cascade_uniforms = cascade_uniforms_array[in.cascade_index + 1];
        let block_size: vec2u = cascade_uniforms.texture_size / cascade_uniforms.probe_size;

        var radiance = vec4f(0.0f);
        for (var i: u32 = 0; i < 4; i++)
        {
            let upper_cascade_direction_index: u32 = direction_index + i;
            let block_x: u32 = upper_cascade_direction_index % cascade_uniforms.probe_size;
            let block_y: u32 = upper_cascade_direction_index / cascade_uniforms.probe_size;

            let sample_uv =
                vec2f(f32(block_size.x * block_x), f32(block_size.y * block_y)) / vec2f(cascade_uniforms.texture_size) +
                vec2f(uv / f32(cascade_uniforms.probe_size));

            let next_interval = textureSampleLevel(cascade_texture, radiance_sampler, sample_uv, 0);
            radiance += merge_intervals(current_interval, next_interval);
        }

        return radiance / 4.0f;
    }
    return current_interval;
}

/*
 * Composites the lowest cascade level onto the screen. This is done with a simple
 * average of the four bilinear interpolated directions from the lowest cascade.
*/
@fragment
fn lighting_frag(in: VertexData) -> @location(0) vec4f {
    let cascade_uniforms = cascade_uniforms_array[in.cascade_index];
    let block_size = cascade_uniforms.texture_size / cascade_uniforms.probe_size;

    var radiance = vec4f(0.0f);
    for (var i = 0; i < 4; i++)
    {
        let sample_uv =
            vec2f(f32(i % 2) * f32(block_size.x), f32(i / 2) * f32(block_size.y)) / vec2f(cascade_uniforms.texture_size) +
            in.uv / 2.0f;
        let interval = textureSampleLevel(cascade_texture, radiance_sampler, sample_uv, 0);
        radiance += interval;
    }

    return radiance / 4.0f;
}
