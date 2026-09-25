#include "WorldRayQuery.slang"
Texture2D<float4> positions;
Texture2D<float4> voxels;
RWTexture2D<float> visibility;
uniform float4 eye;
uniform float4 sun;
uniform uint2 extent;
uniform float2 sampling; // angular radius, frame
uniform float shadowRange; // Blocks; zero traces the full scene range.
uniform uint shadowSamples;
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
            const float phase=(u+v)*6.2831853;
            const float phase_cos=cos(phase),phase_sin=sin(phase);
            const float2 disk_offsets[4]={float2(0,0),float2(.75,0),float2(-.375,.649519),float2(-.375,-.649519)};
            const float2 ultra_offsets[4]={float2(0,.9),float2(0,-.9),float2(.636396,.636396),float2(-.636396,-.636396)};
            const float disk_radius=tan(sampling.x);
            float conservative=1;
            // High retains the original four conservative taps. Lower tiers
            // explicitly trade penumbra coverage for traversal cost.
            for(uint tap=0;tap<clamp(shadowSamples,1u,8u);++tap) {
                float2 offset;
                if(tap<4)offset=disk_offsets[tap];else offset=ultra_offsets[tap-4];
                const float2 rotated=float2(offset.x*phase_cos-offset.y*phase_sin,
                    offset.x*phase_sin+offset.y*phase_cos);
                const float3 sample_direction=normalize(direction+disk_radius*(rotated.x*tangent+rotated.y*bitangent));
                // Do not fold a sun-disk sample back across the receiver plane.
                // A below-horizon sample is physically self-occluded.
                if(dot(sample_direction,normal)<=0.001) {
                    conservative=0;
                    break;
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
                    worldPosition+normal*0.002+sample_direction*0.0005:
                    worldPosition-normal*0.001;
                const float3 ignoredBlock=planeReceiver?float3(1e30):
                    floor(worldPosition-normal*0.001);
                // Trace to the full scene range: a shadow-distance-truncated miss
                // must not read as sunlit inside large enclosures.
                const float fade=shadowRange>0?shadowRange:raySettings.y;
                const WorldRayHit hit=world_trace_ray(origin,sample_direction,raySettings.y,true,ignoredBlock);
                const bool playerHit=hit.hit&&(hit.flags>>31)!=0;
                // Voxel self-hits are removed by ignoredBlock. Map triangles
                // still need only a tiny numerical guard; nearby geometry is
                // otherwise a real occluder at a corner.
                const bool selfHit=mapReceiver&&hit.hit&&!playerHit&&hit.distance<.00025;
                const float sample_visibility=hit.hit&&!selfHit?saturate((hit.distance/fade-.875)*8):1;
                conservative=min(conservative,sample_visibility);
                // Zero is absorbing for min: remaining rays cannot change it.
                if(conservative==0)break;
            }
            result=conservative;
        }
    }
    visibility[id.xy]=result;
}
