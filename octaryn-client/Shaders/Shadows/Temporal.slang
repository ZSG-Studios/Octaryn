#include "../Lighting/Surface.slang"
Texture2D<float> currentShadow;
Texture2D<float4> positions;
Texture2D<float4> voxels;
Texture2D<float2> previousShadow;
Texture2D<float4> previousPositions;
Texture2D<float4> previousVoxels;
RWTexture2D<float2> shadowHistory;
RWTexture2D<float4> positionHistory;
RWTexture2D<float4> voxelHistory;
RWTexture2D<float> outputShadow;
uniform float4 eye;
uniform float4 previousEye;
uniform float4 previousRight;
uniform float4 previousUp;
uniform float4 previousForward;
uniform float4 previousProjection;
uniform float4 options; // extent, valid, hysteresis
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=uint2(options.xy)))return;
    float4 surface=positions[id.xy];float4 packed=voxels[id.xy];
    float3 world=surface.xyz+eye.xyz;
    float sample=currentShadow[id.xy],age=1;
    if(options.z!=0 && surface.w>0) {
        float3 relative=world-previousEye.xyz;
        float z=dot(relative,previousForward.xyz);
        float2 clip=float2(dot(relative,previousRight.xyz),dot(relative,previousUp.xyz))*previousProjection.xy/max(z,.001);
        clip+=float2(previousRight.w,previousUp.w);
        int2 pixel=int2(floor((clip*float2(.5,-.5)+.5)*options.xy));
        if(z>0 && all(pixel>=0) && all(pixel<int2(options.xy))) {
            float4 old=previousPositions[pixel];
            float tolerance=max(.025,length(surface.xyz)*.0005);
            if(old.w>0 && distance(old.xyz,world)<tolerance && all(previousVoxels[pixel]==packed)) {
                float2 history=previousShadow[pixel];
                age=min(history.y+1,32);
                sample=lerp(sample,history.x,min(options.w,(age-1)/age));
            }
        }
    }
    shadowHistory[id.xy]=float2(sample,age);
    outputShadow[id.xy]=sample;
    positionHistory[id.xy]=float4(world,surface.w);
    voxelHistory[id.xy]=packed;
}
