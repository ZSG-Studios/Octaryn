#include "../Lighting/Surface.slang"
#include "MapReflections.slang"
Texture2D<float4> colors;
Texture2D<float4> positions;
Texture2D<float4> voxels;
Texture2D<float4> materials;
RWTexture2D<float4> reflectionSample;
uniform float4 eye;
uniform float4 dimensions; // width, height, frame index, reserved
uniform float4 sun;
uniform float4 lighting;
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=uint2(dimensions.xy)))return;
    uint voxel=lighting_voxel(voxels[id.xy]);
    float4 position=positions[id.xy];
    if(!lighting_is_map(voxel) || position.w<=0) {
        reflectionSample[id.xy]=0;return;
    }
    float3 normal=lighting_normal(voxel);
    float3 view=-position.xyz/max(length(position.xyz),1e-5);
    float metallic=saturate(materials[id.xy].g),roughness=saturate(materials[id.xy].r);
    uint count=map_reflection_sample_count(roughness);
    uint sample=uint(dimensions.z)%count;
    // One native pixel sample per frame; its four-frame mean matches reference quadrature.
    float3 radiance=map_reflection_sample(position.xyz+eye.xyz,normal,view,colors[id.xy].rgb,
        metallic,roughness,sun,float4(lighting.xyz,0),sample,count);
    reflectionSample[id.xy]=float4(radiance,1);
}
