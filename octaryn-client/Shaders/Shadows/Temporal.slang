#include "../Lighting/Surface.slang"
#include "../Lighting/HistoryConfidence.slang"
Texture2D<float> currentShadow;
Texture2D<float4> positions;
Texture2D<float4> voxels;
Texture2D<float4> previousShadow;
Texture2D<float4> previousPositions;
Texture2D<float4> previousVoxels;
RWTexture2D<float4> shadowHistory;
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
    // Tent prefilter of the raw single-ray result. Same-voxel, nearby taps only,
    // so the sun edge softens by ~1px without dilating shadows or holes.
    // Match faces, not whole voxel ids: a shadow line crossing a flat wall spans
    // several blocks and must still filter, otherwise block seams stay aliased.
    uint face=lighting_voxel(packed)&7u;
    float sample=0,sampleWeight=0,sampleMin=1,sampleMax=0;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
        int2 tap=int2(id.xy)+int2(x,y);
        if(any(tap<0)||any(tap>=int2(options.xy)))continue;
        float4 neighbor=positions[tap];
        if(neighbor.w<=0||(lighting_voxel(voxels[tap])&7u)!=face||distance(neighbor.xyz,surface.xyz)>.2)continue;
        float weight=(x==0&&y==0)?4:(x==0||y==0)?2:1;
        float value=currentShadow[tap];
        sample+=value*weight;sampleWeight+=weight;
        sampleMin=min(sampleMin,value);sampleMax=max(sampleMax,value);
    }
    // Unanimous neighborhoods stay pixel-crisp; only mixed penumbrae average.
    // Blurring clean 0/1 regions is what dulled hard voxel shadows.
    sample=(sampleWeight>0&&sampleMax-sampleMin>.05)?sample/sampleWeight:currentShadow[id.xy];
    float age=1,fast=sample;
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
                float4 history=previousShadow[pixel];
                // Occluders can move while the receiver remains unchanged.
                // Clip stale visibility to nearby current samples on this surface.
                float low=sample,high=sample,mean=0,squares=0,oldMean=0,samples=0;
                for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
                    int2 tap=int2(id.xy)+int2(x,y);
                    if(any(tap<0) || any(tap>=int2(options.xy)))continue;
                    float4 neighbor=positions[tap];
                    if(neighbor.w<=0 || (lighting_voxel(voxels[tap])&7u)!=face ||
                        distance(neighbor.xyz,surface.xyz)>max(.2,length(surface.xyz)*.005))continue;
                    float value=currentShadow[tap];low=min(low,value);high=max(high,value);
                    int2 oldTap=pixel+int2(x,y);
                    if(any(oldTap<0) || any(oldTap>=int2(options.xy)))continue;
                    float4 oldPosition=previousPositions[oldTap];
                    if(oldPosition.w<=0 || any(previousVoxels[oldTap]!=packed) ||
                        distance(oldPosition.xyz,neighbor.xyz+eye.xyz)>tolerance)continue;
                    mean+=value;squares+=value*value;oldMean+=previousShadow[oldTap].z;++samples;
                }
                history.x=clamp(history.x,low,high);
                float confidence=1;
                if(samples>=3) {
                    mean/=samples;squares/=samples;oldMean/=samples;
                    confidence=lighting_history_confidence(float3(mean),float3(oldMean),float3(squares-mean*mean),samples);
                }
                age=min(history.y+1,12);
                float historyWeight=min(options.w,(age-1)/age)*confidence;
                age=1/(1-historyWeight);fast=lerp(sample,history.z,.5*confidence);
                sample=lerp(sample,history.x,historyWeight);
            }
        }
    }
    shadowHistory[id.xy]=float4(sample,age,fast,0);
    outputShadow[id.xy]=sample;
    positionHistory[id.xy]=float4(world,surface.w);
    voxelHistory[id.xy]=packed;
}
