#include "WorldRayQuery.slang"
Texture2D<float4> positions;
Texture2D<float4> voxels;
RWTexture2D<float> visibility;
uniform float4 eye;
uniform float4 sun;
uniform uint2 extent;
uniform float2 sampling; // angular radius, frame
uniform float shadowRange; // Blocks; zero traces the full scene range.
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
        float3 direction=normalize(sun.xyz);
        if(dot(normal,direction)>0) {
            // Static IGN sun-disk jitter (no per-frame rotation). A rotating disk
            // feeds history a different sample every frame, so edges shimmer
            // forever instead of converging; the drifting sun already resets
            // history every few seconds. Fixed per-pixel disk offsets converge
            // to a stable soft edge, and the tent filter smooths the pattern.
            // The ray origin stays view-stable (normal offset only, NVIDIA DXR
            // method) so caster projection cannot swim when the camera rotates.
            float u=frac(52.9829189*frac(dot(float2(id.xy),float2(.06711056,.00583715))));
            float v=frac(52.9829189*frac(dot(float2(id.yx)+19,float2(.06711056,.00583715))));
            float3 tangent=normalize(cross(abs(direction.y)<.95?float3(0,1,0):float3(1,0,0),direction));
            float3 bitangent=cross(direction,tangent);
            direction=normalize(direction+tan(sampling.x)*sqrt(u)*(cos(v*6.2831853)*tangent+sin(v*6.2831853)*bitangent));
            // Fold sub-horizon jitter back above the surface. Below-horizon rays
            // can only self-hit the originating plane, never real occluders; the
            // fold lets the distance gate shrink to numerical slop. A wide gate
            // erases legitimate concave-corner occluders, leaking sun along
            // floor/wall seams in lines that track the sun (NVIDIA DXR
            // self-intersection-avoidance: connection-ray endpoint offsets).
            // The origin itself stays a view-stable normal offset so caster
            // projection cannot swim when the camera rotates.
            float horizon=dot(direction,normal);
            if(horizon<0.02)direction=normalize(direction+normal*(0.02-horizon));
            float3 origin=position.xyz+eye.xyz+normal*0.002;
            // Trace to the full scene range: a shadow-distance-truncated miss must
            // not read as sunlit inside large enclosures, where grazing rays easily
            // exceed the user's shadow distance before reaching a wall. The fade
            // below still softens the shadow-distance boundary visually.
            const float fade=shadowRange>0?shadowRange:raySettings.y;
            const WorldRayHit hit=world_trace_ray(origin,direction,raySettings.y,true);
            bool playerHit=hit.hit&&(hit.flags>>31)!=0;
            // The 2mm normal offset plus the horizon fold means a genuine self-hit
            // is numerical slop only; 5mm also erased real near occluders, which
            // leaked sun along block outlines that tracked the sun's angle.
            bool selfHit=hit.hit&&!playerHit&&hit.distance<.0025;
            result=hit.hit&&!selfHit?saturate((hit.distance/fade-.875)*8):1;
        }
    }
    visibility[id.xy]=result;
}
