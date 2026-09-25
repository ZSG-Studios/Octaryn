// Ported from upstream Octaryn 3557cbf; preserved material behavior.
static const float kTerrainPomDepth = 0.80;
static const float kTerrainPomDistance = 32.0;
static const int kTerrainPomQuality = 128;

struct ParallaxSample
{
    float2 uv;
    float depth;
    float3 slope_normal;
    float slope_blend;
};

float terrain_face_edge_weight(Texture2DArray<float4> atlas, float2 uv)
{
    float2 tile_size = atlas_dimensions(atlas);
    float texel = 1.0 / max(min(tile_size.x, tile_size.y), 1.0);
    float2 local_uv = frac(uv);
    float2 edge_distance = min(local_uv, float2(1.0) - local_uv);
    float edge = min(edge_distance.x, edge_distance.y);
    return smoothstep(max(texel, 0.050), max(texel * 2.0, 0.090), edge);
}

float sample_parallax_height(Texture2DArray<float4> normal_atlas, float2 uv, uint voxel, float2 dx, float2 dy)
{
    return sample_face_tiled_atlas_grad(normal_atlas, uv, voxel, dx, dy).a;
}

float sample_parallax_local_height(Texture2DArray<float4> normal_atlas,
                                   float2 tile_origin,
                                   float2 local_uv,
                                   uint voxel,
                                   float2 dx,
                                   float2 dy)
{
    if (any(local_uv <= float2(0.0)) || any(local_uv >= float2(1.0)))
    {
        return 1.0;
    }
    float2 tile_size = atlas_dimensions(normal_atlas);
    float2 tile_pixel = 0.5 / tile_size;
    float2 clamped_uv = clamp(local_uv, tile_pixel, float2(1.0) - tile_pixel);
    return sample_parallax_height(normal_atlas, tile_origin + clamped_uv, voxel, dx, dy);
}

float parallax_bayer_dither(float2 pixel)
{
    int2 p = int2(fmod(pixel, float2(4.0)));
    int index = p.x + p.y * 4;
    const float bayer[16] = {
        0.0, 8.0, 2.0, 10.0,
        12.0, 4.0, 14.0, 6.0,
        3.0, 11.0, 1.0, 9.0,
        15.0, 7.0, 13.0, 5.0
    };
    return (bayer[index] + 0.5) / 16.0;
}

float3 get_parallax_slope_normal(Texture2DArray<float4> normal_atlas,
                               float2 tile_origin,
                               float2 local_uv,
                               float trace_height,
                               float3 view_vector,
                               uint voxel,
                               float2 dx,
                               float2 dy)
{
    float2 tile_size = atlas_dimensions(normal_atlas);
    float2 tile_pixel = 1.0 / tile_size;
    float2 snapped = floor(local_uv * tile_size) * tile_pixel;
    float2 tex_offset = local_uv - (snapped + 0.5 * tile_pixel);
    float2 step_sign = sign(tex_offset);
    float2 view_sign = sign(view_vector.xy);
    bool y_dominant = abs(tex_offset.x) < abs(tex_offset.y);
    float2 sample_x;
    float2 sample_y;

    if (y_dominant)
    {
        sample_x = local_uv - float2(tile_pixel.x * view_sign.x, 0.0);
        sample_y = local_uv + float2(0.0, step_sign.y * tile_pixel.y);
    }
    else
    {
        sample_x = local_uv + float2(tile_pixel.x * step_sign.x, 0.0);
        sample_y = local_uv - float2(0.0, view_sign.y * tile_pixel.y);
    }

    float height_x = sample_parallax_local_height(normal_atlas, tile_origin, sample_x, voxel, dx, dy);
    float height_y = sample_parallax_local_height(normal_atlas, tile_origin, sample_y, voxel, dx, dy);

    if (y_dominant)
    {
        if (!(trace_height > height_y && view_sign.y != step_sign.y))
        {
            if (trace_height > height_x) { return float3(-view_sign.x, 0.0, 0.0); }
            return abs(view_vector.y) > abs(view_vector.x) ? float3(0.0, -view_sign.y, 0.0)
                                                           : float3(-view_sign.x, 0.0, 0.0);
        }
        return float3(0.0, -view_sign.y, 0.0);
    }

    if (!(trace_height > height_x && view_sign.x != step_sign.x))
    {
        if (trace_height > height_y) { return float3(0.0, -view_sign.y, 0.0); }
        return abs(view_vector.y) > abs(view_vector.x) ? float3(0.0, -view_sign.y, 0.0)
                                                       : float3(-view_sign.x, 0.0, 0.0);
    }
    return float3(-view_sign.x, 0.0, 0.0);
}

ParallaxSample apply_terrain_parallax(Texture2DArray<float4> normal_atlas,
                                      float2 uv,
                                      float2 dx,
                                      float2 dy,
                                      float3 view_dir,
                                      float view_distance,
                                      uint voxel, float2 fragmentPosition)
{
    ParallaxSample result;
    result.uv = uv;
    result.depth = 0.0;
    result.slope_normal = float3(0.0, 0.0, 1.0);
    result.slope_blend = 0.0;

    if (is_sprite_voxel(voxel))
    {
        return result;
    }

    uint direction = get_direction(voxel);
    float3 tangent = get_face_tangent(direction);
    float3 bitangent = get_face_bitangent(direction);
    float3 normal = get_normal(voxel);
    float3 view_ts = float3(dot(view_dir, tangent),
                        dot(view_dir, bitangent),
                        max(dot(view_dir, normal), 0.001));
    if (view_ts.z <= 0.001)
    {
        return result;
    }

    float4 initial_normal = sample_face_tiled_atlas_grad(normal_atlas, uv, voxel, dx, dy);
    float2 initial_xy = initial_normal.xy * 2.0 - 1.0;
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

    float2 tile_origin = floor(uv);
    float2 local_uv = frac(uv);
    float2 tile_size = atlas_dimensions(normal_atlas);
    float2 tile_pixel = 0.5 / tile_size;
    float edge_fade = terrain_face_edge_weight(normal_atlas, uv);
    if (edge_fade <= 0.001)
    {
        return result;
    }
    float2 interval = -view_ts.xy * (0.25 * kTerrainPomDepth * (1.0 - fade)) /
                    (view_ts.z * float(kTerrainPomQuality));
    interval = clamp(interval, float2(-0.012), float2(0.012)) * edge_fade;

    float2 previous_local = local_uv;
    float previous_trace = 1.0;
    float previous_height = initial_height;
    float trace_height = 1.0;
    float sampled_height = initial_height;
    float dither = parallax_bayer_dither(fragmentPosition);
    if (sampled_height <= trace_height)
    {
        local_uv += interval * dither;
        if (any(local_uv <= float2(0.0)) || any(local_uv >= float2(1.0)))
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
        if (any(local_uv <= float2(0.0)) || any(local_uv >= float2(1.0)))
        {
            return result;
        }
        trace_height -= inv_quality;
        sampled_height = sample_parallax_local_height(normal_atlas, tile_origin, local_uv, voxel, dx, dy);
    }

    float after = sampled_height - trace_height;
    float before = previous_height - previous_trace;
    float blend = clamp(after / max(after - before, 0.0001), 0.0, 1.0);
    local_uv = lerp(local_uv, previous_local, blend);
    trace_height = lerp(trace_height, previous_trace, blend);
    sampled_height = sample_parallax_local_height(normal_atlas, tile_origin, local_uv, voxel, dx, dy);

    result.uv = tile_origin + clamp(local_uv, tile_pixel, float2(1.0) - tile_pixel);
    result.depth = saturate(1.0 - sampled_height);

    float slope_threshold = max(inv_quality, 1.0 / 255.0);
    if (sampled_height - trace_height > slope_threshold)
    {
        float3 view_vector = float3(-view_ts.xy, -view_ts.z);
        result.slope_normal = get_parallax_slope_normal(normal_atlas,
                                                        tile_origin,
                                                        frac(local_uv),
                                                        trace_height,
                                                        view_vector,
                                                        voxel,
                                                        dx,
                                                        dy);
        result.slope_blend = 0.5 * edge_fade * pow(saturate(1.0 - fade * 2.0), 2.0);
    }

    return result;
}
