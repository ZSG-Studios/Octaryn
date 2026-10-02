#include "WorldRayQuery.slang"
#include "Diagnostics.slang"
#include "../Shadows/Reprojection.slang"
Texture2D<float4> positions;
Texture2D<float4> voxels;
RWTexture2D<float> visibility;
uniform float4 eye;
uniform float4 sun;
uniform uint2 extent;
uniform float2 sampling; // angular radius, frame
uniform float shadowRange; // Blocks; zero traces the full scene range.
uniform uint shadowSamples;
Texture2D<float4> previousShadow;
Texture2D<float> previousPositions;
Texture2D<float4> previousVoxels;
uniform float4 historyOptions; // extent, history valid, reference path
uniform float4 previousEye;
uniform float4 previousRight;
uniform float4 previousUp;
uniform float4 previousForward;
uniform float4 previousProjection;
uint shadow_adaptive_samples(float3 world,uint voxel) {
    uint recovery=min(shadowSamples,4u);
    if(historyOptions.z==0){ray_diagnostic(1);return recovery;}
    float3 relative=world-previousEye.xyz;
    float z=dot(relative,previousForward.xyz);
    float2 clip=float2(dot(relative,previousRight.xyz),dot(relative,previousUp.xyz))*
        previousProjection.xy/max(z,1e-5)+float2(previousRight.w,previousUp.w);
    int2 pixel=int2(floor((clip*float2(.5,-.5)+.5)*historyOptions.xy));
    if(z<=0 || any(pixel<0) || any(pixel>=int2(historyOptions.xy))){ray_diagnostic(1);return recovery;}
    float depth=previousPositions[pixel];
    uint oldVoxel=lighting_voxel(previousVoxels[pixel]);
    if(depth<=0 || dot(lighting_normal(oldVoxel),lighting_normal(voxel))<.95){ray_diagnostic(1);return recovery;}
    float2 oldClip=((float2(pixel)+.5)/historyOptions.xy*2-1)*float2(1,-1);
    oldClip-=float2(previousRight.w,previousUp.w);
    float3 ray=previousForward.xyz+previousRight.xyz*(oldClip.x/previousProjection.x)+
        previousUp.xyz*(oldClip.y/previousProjection.y);
    float3 old=previousEye.xyz+ray*depth;
    if(!shadow_history_position(old,world,voxel,previousEye.xyz,ray,max(.025,z*.0005))){ray_diagnostic(1);return recovery;}
    ray_diagnostic(3);
    float4 history=previousShadow[pixel];
    float instability=max(history.x*(1-history.x),abs(history.x-history.z));
    return history.y>=4 && instability<.03?1u:min(shadowSamples,2u);
}
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
        if(is_sprite_voxel(voxel) && !world_plant_voxel(voxel) && dot(normal,sun.xyz)<0)normal=-normal;
        float3 direction=normalize(sun.xyz);
        if(dot(normal,direction)>0 || lighting_is_map(voxel)) {
            ray_diagnostic(0);
            // The reference retains its conservative disk. Adaptive sampling
            // integrates disk visibility over time with a low-discrepancy phase.
            float u=frac(52.9829189*frac(dot(float2(id.xy),float2(.06711056,.00583715))));
            float v=frac(52.9829189*frac(dot(float2(id.yx)+19,float2(.06711056,.00583715))));
            float3 tangent=normalize(cross(abs(direction.y)<.95?float3(0,1,0):float3(1,0,0),direction));
            float3 bitangent=cross(direction,tangent);
            const float phase=(u+v)*6.2831853;
            const float phase_cos=cos(phase),phase_sin=sin(phase);
            const float2 disk_offsets[4]={float2(0,0),float2(.75,0),float2(-.375,.649519),float2(-.375,-.649519)};
            const float2 ultra_offsets[4]={float2(0,.9),float2(0,-.9),float2(.636396,.636396),float2(-.636396,-.636396)};
            const float disk_radius=tan(sampling.x);
            float conservative=1,total=0;
            const bool reference=historyOptions.w!=0;
            const uint count=reference?clamp(shadowSamples,1u,8u):shadow_adaptive_samples(position.xyz+eye.xyz,voxel);
            // High retains the original four conservative taps. Lower tiers
            // explicitly trade penumbra coverage for traversal cost.
            for(uint tap=0;tap<count;++tap) {
                float2 offset;
                if(reference) {
                    if(tap<4)offset=disk_offsets[tap];else offset=ultra_offsets[tap-4];
                } else {
                    float sequence=float(uint(sampling.y)*4+tap);
                    float radius=sqrt(frac(u+sequence*.754877666));
                    float angle=6.2831853*frac(v+sequence*.569840296);
                    offset=radius*float2(cos(angle),sin(angle));
                }
                const float2 rotated=float2(offset.x*phase_cos-offset.y*phase_sin,
                    offset.x*phase_sin+offset.y*phase_cos);
                const float3 sample_direction=normalize(direction+disk_radius*(rotated.x*tangent+rotated.y*bitangent));
                // Do not fold a sun-disk sample back across the receiver plane.
                // A below-horizon sample is physically self-occluded.
                if(dot(sample_direction,normal)<=0.001 && !lighting_is_map(voxel)) {
                    conservative=0;
                    if(reference)break;
                    continue;
                }
                const float3 worldPosition=position.xyz+eye.xyz;
                // Voxel receivers start just inside their own block and skip
                // only that source block.  An outward-only bias can put a
                // grazing ray in the floating wedge at a shared corner, where
                // both adjacent faces miss.  Starting inside keeps the ray on
                // the closed voxel side while the ignored block prevents a
                // self-hit; neighboring blocks remain full occluders.
                const bool mapReceiver=lighting_is_map(voxel);
                const bool planeReceiver=mapReceiver || world_plant_voxel(voxel);
                const float3 origin=planeReceiver?
                    worldPosition+normal*(dot(normal,sample_direction)<0?-0.002:0.002)+sample_direction*0.0005:
                    worldPosition-normal*0.001;
                const float3 ignoredBlock=planeReceiver?float3(1e30):
                    floor(worldPosition-normal*0.001);
                // Trace to the full scene range: a shadow-distance-truncated miss
                // must not read as sunlit inside large enclosures.
                const float fade=shadowRange>0?shadowRange:raySettings.y;
                ray_diagnostic(2);
                const WorldRayHit hit=world_trace_ray(origin,sample_direction,raySettings.y,true,ignoredBlock);
                const bool playerHit=hit.hit&&(hit.flags>>31)!=0;
                // Voxel self-hits are removed by ignoredBlock. Map triangles
                // still need only a tiny numerical guard; nearby geometry is
                // otherwise a real occluder at a corner.
                const bool selfHit=mapReceiver&&hit.hit&&!playerHit&&hit.distance<.00025;
                const float sample_visibility=hit.hit&&!selfHit?saturate((hit.distance/fade-.875)*8):1;
                conservative=min(conservative,sample_visibility);
                total+=sample_visibility;
                // Zero is absorbing for min: remaining rays cannot change it.
                if(reference && conservative==0)break;
            }
            result=reference?conservative:total/float(count);
        }
    }
    visibility[id.xy]=result;
}
