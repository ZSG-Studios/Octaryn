const float kTerrainPomDepth = 0.80;
const float kTerrainPomDistance = 32.0;
const int kTerrainPomQuality = 128;

struct ParallaxSample
{
    vec2 uv;
    float depth;
    vec3 slope_normal;
    float slope_blend;
};

float terrain_face_edge_weight(sampler2DArray atlas, vec2 uv)
{
    vec2 tile_size = vec2(textureSize(atlas, 0).xy);
    float texel = 1.0 / max(min(tile_size.x, tile_size.y), 1.0);
    vec2 local_uv = fract(uv);
    vec2 edge_distance = min(local_uv, vec2(1.0) - local_uv);
    float edge = min(edge_distance.x, edge_distance.y);
    return smoothstep(max(texel, 0.050), max(texel * 2.0, 0.090), edge);
}

float sample_parallax_height(sampler2DArray normal_atlas, vec2 uv, uint voxel, vec2 dx, vec2 dy)
{
    return sample_face_tiled_atlas_grad(normal_atlas, uv, voxel, dx, dy).a;
}

float sample_parallax_local_height(sampler2DArray normal_atlas,
                                   vec2 tile_origin,
                                   vec2 local_uv,
                                   uint voxel,
                                   vec2 dx,
                                   vec2 dy)
{
    if (any(lessThanEqual(local_uv, vec2(0.0))) || any(greaterThanEqual(local_uv, vec2(1.0))))
    {
        return 1.0;
    }
    vec2 tile_size = vec2(textureSize(normal_atlas, 0).xy);
    vec2 tile_pixel = 0.5 / tile_size;
    vec2 clamped_uv = clamp(local_uv, tile_pixel, vec2(1.0) - tile_pixel);
    return sample_parallax_height(normal_atlas, tile_origin + clamped_uv, voxel, dx, dy);
}

float parallax_bayer_dither(vec2 pixel)
{
    ivec2 p = ivec2(mod(pixel, vec2(4.0)));
    int index = p.x + p.y * 4;
    const float bayer[16] = float[16](
        0.0, 8.0, 2.0, 10.0,
        12.0, 4.0, 14.0, 6.0,
        3.0, 11.0, 1.0, 9.0,
        15.0, 7.0, 13.0, 5.0
    );
    return (bayer[index] + 0.5) / 16.0;
}

vec3 get_parallax_slope_normal(sampler2DArray normal_atlas,
                               vec2 tile_origin,
                               vec2 local_uv,
                               float trace_height,
                               vec3 view_vector,
                               uint voxel,
                               vec2 dx,
                               vec2 dy)
{
    vec2 tile_size = vec2(textureSize(normal_atlas, 0).xy);
    vec2 tile_pixel = 1.0 / tile_size;
    vec2 snapped = floor(local_uv * tile_size) * tile_pixel;
    vec2 tex_offset = local_uv - (snapped + 0.5 * tile_pixel);
    vec2 step_sign = sign(tex_offset);
    vec2 view_sign = sign(view_vector.xy);
    bool y_dominant = abs(tex_offset.x) < abs(tex_offset.y);
    vec2 sample_x;
    vec2 sample_y;

    if (y_dominant)
    {
        sample_x = local_uv - vec2(tile_pixel.x * view_sign.x, 0.0);
        sample_y = local_uv + vec2(0.0, step_sign.y * tile_pixel.y);
    }
    else
    {
        sample_x = local_uv + vec2(tile_pixel.x * step_sign.x, 0.0);
        sample_y = local_uv - vec2(0.0, view_sign.y * tile_pixel.y);
    }

    float height_x = sample_parallax_local_height(normal_atlas, tile_origin, sample_x, voxel, dx, dy);
    float height_y = sample_parallax_local_height(normal_atlas, tile_origin, sample_y, voxel, dx, dy);

    if (y_dominant)
    {
        if (!(trace_height > height_y && view_sign.y != step_sign.y))
        {
            if (trace_height > height_x) { return vec3(-view_sign.x, 0.0, 0.0); }
            return abs(view_vector.y) > abs(view_vector.x) ? vec3(0.0, -view_sign.y, 0.0)
                                                           : vec3(-view_sign.x, 0.0, 0.0);
        }
        return vec3(0.0, -view_sign.y, 0.0);
    }

    if (!(trace_height > height_x && view_sign.x != step_sign.x))
    {
        if (trace_height > height_y) { return vec3(0.0, -view_sign.y, 0.0); }
        return abs(view_vector.y) > abs(view_vector.x) ? vec3(0.0, -view_sign.y, 0.0)
                                                       : vec3(-view_sign.x, 0.0, 0.0);
    }
    return vec3(-view_sign.x, 0.0, 0.0);
}

ParallaxSample apply_terrain_parallax(sampler2DArray normal_atlas,
                                      vec2 uv,
                                      vec2 dx,
                                      vec2 dy,
                                      vec3 view_dir,
                                      float view_distance,
                                      uint voxel)
{
    ParallaxSample result;
    result.uv = uv;
    result.depth = 0.0;
    result.slope_normal = vec3(0.0, 0.0, 1.0);
    result.slope_blend = 0.0;

    if (is_sprite_voxel(voxel))
    {
        return result;
    }

    uint direction = get_direction(voxel);
    vec3 tangent = get_face_tangent(direction);
    vec3 bitangent = get_face_bitangent(direction);
    vec3 normal = get_normal(voxel);
    vec3 view_ts = vec3(dot(view_dir, tangent),
                        dot(view_dir, bitangent),
                        max(dot(view_dir, normal), 0.001));
    if (view_ts.z <= 0.001)
    {
        return result;
    }

    vec4 initial_normal = sample_face_tiled_atlas_grad(normal_atlas, uv, voxel, dx, dy);
    vec2 initial_xy = initial_normal.xy * 2.0 - 1.0;
    float initial_height = initial_normal.a;
    float inv_quality = 1.0 / float(kTerrainPomQuality);
    if (initial_height >= 1.0 - inv_quality || initial_xy.x + initial_xy.y <= -1.999)
    {
        return result;
    }

    float distance_fade = saturate(pow(view_distance / kTerrainPomDistance, 2.0));
    float height_fade = pow(initial_height, 64.0);
    float fade = saturate(distance_fade + height_fade);
    if (fade >= 1.0)
    {
        return result;
    }

    vec2 tile_origin = floor(uv);
    vec2 local_uv = fract(uv);
    vec2 tile_size = vec2(textureSize(normal_atlas, 0).xy);
    vec2 tile_pixel = 0.5 / tile_size;
    float edge_fade = terrain_face_edge_weight(normal_atlas, uv);
    if (edge_fade <= 0.001)
    {
        return result;
    }
    vec2 interval = -view_ts.xy * (0.25 * kTerrainPomDepth * (1.0 - fade)) /
                    (view_ts.z * float(kTerrainPomQuality));
    interval = clamp(interval, vec2(-0.012), vec2(0.012)) * edge_fade;

    vec2 previous_local = local_uv;
    float previous_trace = 1.0;
    float previous_height = initial_height;
    float trace_height = 1.0;
    float sampled_height = initial_height;
    float dither = parallax_bayer_dither(gl_FragCoord.xy);
    if (sampled_height <= trace_height)
    {
        local_uv += interval * dither;
        if (any(lessThanEqual(local_uv, vec2(0.0))) || any(greaterThanEqual(local_uv, vec2(1.0))))
        {
            return result;
        }
        trace_height -= inv_quality * dither;
        sampled_height = sample_parallax_local_height(normal_atlas, tile_origin, local_uv, voxel, dx, dy);
    }

    for (int i = 0; i < kTerrainPomQuality; ++i)
    {
        if (sampled_height > trace_height)
        {
            break;
        }

        previous_local = local_uv;
        previous_trace = trace_height;
        previous_height = sampled_height;

        local_uv += interval;
        if (any(lessThanEqual(local_uv, vec2(0.0))) || any(greaterThanEqual(local_uv, vec2(1.0))))
        {
            return result;
        }
        trace_height -= inv_quality;
        sampled_height = sample_parallax_local_height(normal_atlas, tile_origin, local_uv, voxel, dx, dy);
    }

    float after = sampled_height - trace_height;
    float before = previous_height - previous_trace;
    float blend = clamp(after / max(after - before, 0.0001), 0.0, 1.0);
    local_uv = mix(local_uv, previous_local, blend);
    trace_height = mix(trace_height, previous_trace, blend);
    sampled_height = sample_parallax_local_height(normal_atlas, tile_origin, local_uv, voxel, dx, dy);

    result.uv = tile_origin + clamp(local_uv, tile_pixel, vec2(1.0) - tile_pixel);
    result.depth = saturate(1.0 - sampled_height);

    float slope_threshold = max(inv_quality, 1.0 / 255.0);
    if (sampled_height - trace_height > slope_threshold)
    {
        vec3 view_vector = vec3(-view_ts.xy, -view_ts.z);
        result.slope_normal = get_parallax_slope_normal(normal_atlas,
                                                        tile_origin,
                                                        fract(local_uv),
                                                        trace_height,
                                                        view_vector,
                                                        voxel,
                                                        dx,
                                                        dy);
        result.slope_blend = 0.5 * edge_fade * pow(saturate(1.0 - fade * 2.0), 2.0);
    }

    return result;
}
