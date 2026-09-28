// Sphere AABBs conservatively bound selected clusters. Never use a nearest-depth
// reduction: a single uncovered pixel must prevent occluding its entire region.
struct OcclusionCluster {
    uint4 topology;uint4 geometry;uint flags;float3 center;float radius;float error;
};
StructuredBuffer<OcclusionCluster> occlusionClusters;
StructuredBuffer<uint4> occlusionSelected;
StructuredBuffer<uint> occlusionSelectionCounters;
StructuredBuffer<uint64_t> occlusionVisibility;
RWStructuredBuffer<uint> occlusionFlags;
RWStructuredBuffer<uint> occlusionCounters;
Texture2D<float> occlusionPyramid;
Texture2D<float> occlusionSource;
RWTexture2D<float> occlusionTarget;
uniform float4 occlusionView[5];
uniform uint4 occlusionSettings; // selected capacity, viewport width/height, history enabled
uniform uint4 occlusionPyramidSize; // padded width/height, mip count, reserved

uint occlusion_index(uint3 id) {return id.x+id.y*(65535u*64u);}
bool hidden(OcclusionCluster cluster) {
    float2 lower=float2(1e20),upper=float2(-1e20);float nearest=1;
    [unroll]for(uint corner=0;corner<8;++corner) {
        float3 signs=float3(corner&1?1:-1,corner&2?1:-1,corner&4?1:-1);
        float3 relative=cluster.center+signs*cluster.radius-occlusionView[0].xyz;
        float z=dot(relative,occlusionView[3].xyz);
        // Crossing the near plane or leaving the recorded viewport is uncertain.
        if(z<=.1)return false;
        float2 ndc=float2(dot(relative,occlusionView[1].xyz)*occlusionView[4].x,
                         dot(relative,occlusionView[2].xyz)*occlusionView[4].y)/z;
        ndc+=float2(occlusionView[1].w,occlusionView[2].w);
        float2 screen=(ndc*float2(.5,-.5)+.5)*float2(occlusionSettings.yz);
        lower=min(lower,screen);upper=max(upper,screen);
        nearest=min(nearest,occlusionView[4].z-occlusionView[4].w/z);
    }
    lower=floor(lower-1);upper=ceil(upper+1);
    if(any(lower<0) || any(upper>=float2(occlusionSettings.yz)) || nearest<=0 || nearest>=1)return false;
    float2 extent=upper-lower+1;
    uint mip=min(uint(floor(log2(max(1,max(extent.x,extent.y))))),occlusionPyramidSize.z-1);
    uint2 first=uint2(lower)>>mip,last=uint2(upper)>>mip;
    float farthest=0;
    // Rectangle bounds can straddle up to three texels per axis at this mip.
    for(uint y=first.y;y<=last.y;++y)for(uint x=first.x;x<=last.x;++x)
        farthest=max(farthest,occlusionPyramid.Load(int3(x,y,mip)));
    return nearest>farthest+1e-5;
}
[shader("compute")]
[numthreads(64,1,1)]
void reset_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=occlusion_index(dispatch);if(id<4)occlusionCounters[id]=0;
}
[shader("compute")]
[numthreads(64,1,1)]
void classify_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=occlusion_index(dispatch);if(id>=min(occlusionSelectionCounters[0],occlusionSettings.x))return;
    bool occluded=occlusionSettings.w!=0 && hidden(occlusionClusters[occlusionSelected[id].x]);
    occlusionFlags[id]=occluded?2:1;
    if(occluded)InterlockedAdd(occlusionCounters[0],1);
    if(id==0 && occlusionSettings.w==0)occlusionCounters[3]=1;
}
[shader("compute")]
[numthreads(64,1,1)]
void retest_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=occlusion_index(dispatch);
    if(id>=min(occlusionSelectionCounters[0],occlusionSettings.x) || occlusionFlags[id]!=2)return;
    bool occluded=hidden(occlusionClusters[occlusionSelected[id].x]);
    occlusionFlags[id]=occluded?4:3;InterlockedAdd(occlusionCounters[occluded?2:1],1);
}
[shader("compute")]
[numthreads(8,8,1)]
void depth_main(uint2 pixel:SV_DispatchThreadID) {
    uint width,height;occlusionTarget.GetDimensions(width,height);if(any(pixel>=uint2(width,height)))return;
    float depth=1;
    if(all(pixel<occlusionSettings.yz)) {
        uint64_t winner=occlusionVisibility[pixel.y*occlusionSettings.y+pixel.x];
        if(winner!=0)depth=asfloat(~uint(winner>>32));
    }
    occlusionTarget[pixel]=depth;
}
[shader("compute")]
[numthreads(8,8,1)]
void reduce_main(uint2 pixel:SV_DispatchThreadID) {
    uint width,height;occlusionTarget.GetDimensions(width,height);if(any(pixel>=uint2(width,height)))return;
    uint sourceWidth,sourceHeight;occlusionSource.GetDimensions(sourceWidth,sourceHeight);
    uint2 base=pixel*2,last=uint2(sourceWidth,sourceHeight)-1;
    float depth=occlusionSource[min(base,last)];
    depth=max(depth,occlusionSource[min(base+uint2(1,0),last)]);
    depth=max(depth,occlusionSource[min(base+uint2(0,1),last)]);
    depth=max(depth,occlusionSource[min(base+uint2(1,1),last)]);
    occlusionTarget[pixel]=depth;
}
