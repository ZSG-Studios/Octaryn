struct PbrMaterial
{
    vec3 normal;
    float roughness;
    float metallic;
    float f0;
    float emission;
    float material_ao;
    float porosity;
    float subsurface;
};

vec3 get_face_tangent(uint direction)
{
    if (direction == kDirectionSouth) { return vec3(-1.0, 0.0, 0.0); }
    if (direction == kDirectionEast) { return vec3(0.0, 0.0, -1.0); }
    if (direction == kDirectionWest) { return vec3(0.0, 0.0, 1.0); }
    return vec3(1.0, 0.0, 0.0);
}

vec3 get_face_bitangent(uint direction)
{
    if (direction == kDirectionUp) { return vec3(0.0, 0.0, -1.0); }
    if (direction == kDirectionDown) { return vec3(0.0, 0.0, 1.0); }
    return vec3(0.0, -1.0, 0.0);
}

vec3 get_biome_tint(uint atlas_index)
{
    return vec3(1.0);
}

vec4 apply_biome_tint(vec4 albedo, uint voxel)
{
    albedo.rgb *= get_biome_tint(get_index(voxel));
    return albedo;
}

float decode_labpbr_height_depth(vec4 normal_sample)
{
    return saturate(1.0 - normal_sample.a);
}

vec3 decode_labpbr_tangent_normal(vec4 normal_sample)
{
    vec3 normal_strength = normal_sample.xyz + vec3(0.5, 0.5, 0.0);
    normal_strength = pow(normal_strength, vec3(0.70));
    normal_strength -= vec3(0.5, 0.5, 0.0);
    vec2 xy = normal_strength.xy * 2.0 - 1.0;
    float xy_len = length(xy);
    if (xy_len > 1.0)
    {
        xy /= xy_len;
    }
    float z = sqrt(max(1.0 - dot(xy, xy), 0.0));
    return normalize(vec3(xy, max(z, 0.001)));
}

vec3 transform_face_tangent_normal(vec3 tangent_normal, vec3 geometric_normal, uint voxel)
{
    uint direction = get_direction(voxel);
    vec3 tangent = get_face_tangent(direction);
    vec3 bitangent = get_face_bitangent(direction);
    return normalize(mat3(tangent, bitangent, geometric_normal) * tangent_normal);
}

float decode_labpbr_material_ao(vec4 normal_sample)
{
    return saturate(normal_sample.b);
}

float decode_labpbr_roughness(vec4 specular_sample)
{
    float perceptual_smoothness = saturate(specular_sample.r);
    return clamp(pow(1.0 - perceptual_smoothness, 2.0), 0.02, 1.0);
}

float decode_labpbr_metallic(vec4 specular_sample)
{
    return specular_sample.g >= (230.0 / 255.0) ? 1.0 : 0.0;
}

float decode_labpbr_f0(vec4 specular_sample)
{
    if (specular_sample.g >= (230.0 / 255.0))
    {
        return 1.0;
    }
    return clamp(specular_sample.g, 0.02, 0.90);
}

float decode_labpbr_emission(vec4 specular_sample)
{
    return specular_sample.a >= (255.0 / 255.0) ? 0.0 : saturate(specular_sample.a / (254.0 / 255.0));
}

float decode_labpbr_porosity(vec4 specular_sample)
{
    return specular_sample.b <= (64.0 / 255.0) ? saturate(specular_sample.b * (255.0 / 64.0)) : 0.0;
}

float decode_labpbr_subsurface(vec4 specular_sample)
{
    return specular_sample.b > (64.0 / 255.0) ? saturate((specular_sample.b * 255.0 - 65.0) / 190.0) : 0.0;
}

PbrMaterial decode_labpbr_material(vec4 normal_sample, vec4 specular_sample, vec3 geometric_normal, uint voxel)
{
    PbrMaterial material;
    material.normal = geometric_normal;
    if (!is_sprite_voxel(voxel))
    {
        material.normal = transform_face_tangent_normal(decode_labpbr_tangent_normal(normal_sample), geometric_normal, voxel);
    }
    material.roughness = decode_labpbr_roughness(specular_sample);
    material.metallic = decode_labpbr_metallic(specular_sample);
    material.f0 = decode_labpbr_f0(specular_sample);
    material.emission = decode_labpbr_emission(specular_sample);
    material.material_ao = decode_labpbr_material_ao(normal_sample);
    material.porosity = decode_labpbr_porosity(specular_sample);
    material.subsurface = decode_labpbr_subsurface(specular_sample);
    return material;
}
