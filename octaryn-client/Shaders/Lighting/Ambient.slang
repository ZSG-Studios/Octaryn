#pragma once
// Original shader_common.glsl ambient and composite.comp.glsl pixel composition.
float get_ambient_sky_scale(float sky_visibility)
{
    float visibility = max(saturate(sky_visibility), 0.42);
    return lerp(0.34, 1.0, sqrt(visibility));
}

float3 get_ambient_light(float ambient_visibility, float sky_visibility)
{
    float raw_visibility = saturate(ambient_visibility);
    float visibility = pow(raw_visibility, 1.08);
    float sky = max(saturate(sky_visibility), 0.42);
    float local_sky = smoothstep(0.0, 0.02, raw_visibility);
    visibility *= local_sky;
    visibility = max(visibility, lerp(0.0, 0.34, sky) * local_sky);
    float sky_scale = get_ambient_sky_scale(sky);
    float day_curve = sqrt(sky);
    float3 night = float3(0.090, 0.090, 0.094);
    float3 day = float3(0.235, 0.240, 0.245);
    return lerp(night, day, day_curve) * visibility * sky_scale;
}

float4 original_composite_color(float4 color, float4 position, uint voxel,
                                float4 material, float sky_visibility, float ambient_strength) {
    if (voxel == 0u && position.w == 0.0) {
        float3 ldr=clamp(color.rgb,float3(0),float3(0.999));
        return float4(ldr/(float3(1)-ldr),1);
    }
    float3 ambient=get_ambient_light(color.a,sky_visibility)*ambient_strength;
    float3 emissive=color.rgb*clamp(material.a,0.0,8.0);
    return float4(max(color.rgb*ambient+emissive,float3(0)),1);
}
