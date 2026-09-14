#include "WorldRayQuery.slang"
Texture2D<float4> positions;
Texture2D<float4> voxels;
RWTexture2D<float> visibility;
uniform float4 eye;
uniform float4 sun;
uniform uint2 extent;
uniform float2 sampling; // angular radius, frame
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=extent))return;
    float4 position=positions[id.xy];
    float result=1;
    if(position.w>0 && sun.w>0 && raySettings.x!=0) {
        uint4 packed=uint4(round(voxels[id.xy]*255));
        uint voxel=packed.x|(packed.y<<8)|(packed.z<<16)|(packed.w<<24);
        float3 normal=get_normal(voxel);
        if((voxel&256)!=0 && dot(normal,sun.xyz)<0)normal=-normal;
        if(dot(normal,sun.xyz)>0) {
            // Interleaved gradient noise plus a golden-ratio temporal rotation.
            float u=frac(52.9829189*frac(dot(float2(id.xy),float2(.06711056,.00583715)))+sampling.y*.61803398875);
            float v=frac(52.9829189*frac(dot(float2(id.yx)+19,float2(.06711056,.00583715)))+sampling.y*.754877666);
            float3 direction=normalize(sun.xyz);
            float3 tangent=normalize(cross(abs(direction.y)<.95?float3(0,1,0):float3(1,0,0),direction));
            float3 bitangent=cross(direction,tangent);
            direction=normalize(direction+tan(sampling.x)*sqrt(u)*(cos(v*6.2831853)*tangent+sin(v*6.2831853)*bitangent));
            result=world_ray_visibility(position.xyz+eye.xyz,normal,direction);
        }
    }
    visibility[id.xy]=result;
}
