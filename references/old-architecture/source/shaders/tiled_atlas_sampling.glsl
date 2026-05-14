vec2 inset_tiled_atlas_uv(sampler2DArray atlas, vec2 wrapped)
{
    vec2 texel = 0.5 / vec2(textureSize(atlas, 0).xy);
    return mix(texel, vec2(1.0) - texel, wrapped);
}

vec2 sharpen_atlas_gradient(vec2 gradient)
{
    return gradient * 0.45;
}

vec4 sample_tiled_atlas_texture(sampler2DArray atlas, vec3 texcoord)
{
    return textureGrad(atlas, texcoord, sharpen_atlas_gradient(dFdx(texcoord.xy)), sharpen_atlas_gradient(dFdy(texcoord.xy)));
}

vec4 sample_sprite_atlas_texture(sampler2DArray atlas, vec3 texcoord)
{
    vec3 wrapped = vec3(clamp(texcoord.xy, vec2(0.0), vec2(1.0)), texcoord.z);
    wrapped.xy = inset_tiled_atlas_uv(atlas, wrapped.xy);
    return textureLod(atlas, wrapped, 0.0);
}

float sample_tiled_atlas_alpha(sampler2DArray atlas, vec3 texcoord)
{
    return sample_tiled_atlas_texture(atlas, texcoord).a;
}

vec2 get_unwrapped_world_face_uv(vec3 world_position, uint direction)
{
    if (direction == kDirectionNorth)
    {
        return vec2(world_position.x, -world_position.y);
    }
    if (direction == kDirectionSouth)
    {
        return vec2(-world_position.x, -world_position.y);
    }
    if (direction == kDirectionEast)
    {
        return vec2(-world_position.z, -world_position.y);
    }
    if (direction == kDirectionWest)
    {
        return vec2(world_position.z, -world_position.y);
    }
    if (direction == kDirectionUp)
    {
        return vec2(world_position.x, -world_position.z);
    }
    return vec2(world_position.x, world_position.z);
}

vec2 get_face_sample_uv(vec3 world_position, uint voxel, vec3 fallback_texcoord)
{
    if (is_sprite_voxel(voxel))
    {
        return clamp(fallback_texcoord.xy, vec2(0.0), vec2(1.0));
    }
    uint direction = get_direction(voxel);
    if (direction == kDirectionNorth ||
        direction == kDirectionSouth ||
        direction == kDirectionEast ||
        direction == kDirectionWest)
    {
        return vec2(fallback_texcoord.x, -fallback_texcoord.y);
    }
    return fallback_texcoord.xy;
}

vec4 sample_face_tiled_atlas_grad(sampler2DArray atlas, vec2 uv, uint voxel, vec2 dx, vec2 dy)
{
    return textureGrad(atlas,
                       vec3(uv, float(get_index(voxel))),
                       sharpen_atlas_gradient(dx),
                       sharpen_atlas_gradient(dy));
}

vec4 sample_face_tiled_atlas_uv(sampler2DArray atlas, vec2 uv, uint voxel)
{
    return sample_face_tiled_atlas_grad(atlas, uv, voxel, dFdx(uv), dFdy(uv));
}

vec4 sample_face_tiled_atlas_lod(sampler2DArray atlas, vec2 uv, uint voxel, float lod)
{
    return textureLod(atlas, vec3(uv, float(get_index(voxel))), lod);
}

vec4 sample_face_tiled_atlas_texture(sampler2DArray atlas, vec3 world_position, uint voxel, vec3 fallback_texcoord)
{
    if (is_sprite_voxel(voxel))
    {
        return sample_sprite_atlas_texture(atlas, fallback_texcoord);
    }

    return sample_face_tiled_atlas_uv(atlas, get_face_sample_uv(world_position, voxel, fallback_texcoord), voxel);
}

float sample_face_tiled_atlas_alpha(sampler2DArray atlas, vec3 world_position, uint voxel, vec3 fallback_texcoord)
{
    return sample_face_tiled_atlas_texture(atlas, world_position, voxel, fallback_texcoord).a;
}
