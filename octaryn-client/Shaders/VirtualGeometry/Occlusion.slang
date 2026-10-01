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
RWStructuredBuffer<uint> occlusionBinArgs;
RWStructuredBuffer<uint> occlusionEarlySoftware;
RWStructuredBuffer<uint> occlusionEarlyHardware;
RWStructuredBuffer<uint> occlusionLateSoftware;
RWStructuredBuffer<uint> occlusionLateHardware;
Texture2D<float> occlusionPyramid;
Texture2D<float> occlusionSource;
RWTexture2D<float> occlusionTarget;
uniform float4 occlusionView[5];
uniform uint4 occlusionSettings; // selected capacity, viewport width/height, history enabled
uniform uint4 occlusionPyramidSize; // padded width/height, mip count, reserved

uint occlusion_index(uint3 id) {return id.x+id.y*(65535u*64u);}
// Shared conservative corner projection: screen AABB, nearest depth, and the
// near/far/viewport uncertainty flags used by both occlusion tests and binning.
struct ClusterBounds {
    float2 lower,upper;float nearest;bool uncertain;
};
ClusterBounds cluster_bounds(OcclusionCluster cluster) {
    ClusterBounds bounds;bounds.lower=float2(1e20);bounds.upper=float2(-1e20);bounds.nearest=1;bounds.uncertain=false;
    [unroll]for(uint corner=0;corner<8;++corner) {
        float3 signs=float3(corner&1?1:-1,corner&2?1:-1,corner&4?1:-1);
        float3 relative=cluster.center+signs*cluster.radius-occlusionView[0].xyz;
        float z=dot(relative,occlusionView[3].xyz);
        // Crossing the near plane or leaving the recorded viewport is uncertain.
        if(z<=.1)bounds.uncertain=true;
        float2 ndc=float2(dot(relative,occlusionView[1].xyz)*occlusionView[4].x,
                         dot(relative,occlusionView[2].xyz)*occlusionView[4].y)/z;
        ndc+=float2(occlusionView[1].w,occlusionView[2].w);
        float2 screen=(ndc*float2(.5,-.5)+.5)*float2(occlusionSettings.yz);
        bounds.lower=min(bounds.lower,screen);bounds.upper=max(bounds.upper,screen);
        bounds.nearest=min(bounds.nearest,occlusionView[4].z-occlusionView[4].w/z);
    }
    if(any(bounds.lower<0) || any(bounds.upper>=float2(occlusionSettings.yz)) ||
        bounds.nearest<=0 || bounds.nearest>=1)bounds.uncertain=true;
    return bounds;
}
bool hidden_bounds(ClusterBounds bounds) {
    if(bounds.uncertain)return false;
    float2 lower=floor(bounds.lower-1),upper=ceil(bounds.upper+1);
    float2 extent=upper-lower+1;
    uint mip=min(uint(floor(log2(max(1,max(extent.x,extent.y))))),occlusionPyramidSize.z-1);
    uint2 first=uint2(lower)>>mip,last=uint2(upper)>>mip;
    float farthest=0;
    // Rectangle bounds can straddle up to three texels per axis at this mip.
    for(uint y=first.y;y<=last.y;++y)for(uint x=first.x;x<=last.x;++x)
        farthest=max(farthest,occlusionPyramid.Load(int3(x,y,mip)));
    return bounds.nearest>farthest+1e-5;
}
bool hidden(OcclusionCluster cluster) {return hidden_bounds(cluster_bounds(cluster));}
// Cluster-level software raster routing: the whole sphere projects inside 4
// pixels, so every triangle satisfies the exact per-triangle extent rule.
bool software_cluster(OcclusionCluster cluster,ClusterBounds bounds) {
    if((cluster.flags&3)!=0 || bounds.uncertain)return false;
    float2 extent=ceil(bounds.upper)-floor(bounds.lower);
    return all(extent<=4) && all(extent>0);
}
[shader("compute")]
[numthreads(64,1,1)]
void reset_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=occlusion_index(dispatch);
    if(id<4)occlusionCounters[id]=0;
    if(id==0) {
        occlusionEarlySoftware[0]=0;occlusionEarlyHardware[0]=0;
        occlusionLateSoftware[0]=0;occlusionLateHardware[0]=0;
    }
}
[shader("compute")]
[numthreads(64,1,1)]
void classify_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=occlusion_index(dispatch);if(id>=min(occlusionSelectionCounters[0],occlusionSettings.x))return;
    OcclusionCluster cluster=occlusionClusters[occlusionSelected[id].x];
    ClusterBounds bounds=cluster_bounds(cluster);
    bool occluded=occlusionSettings.w!=0 && hidden_bounds(bounds);
    occlusionFlags[id]=occluded?2:1;
    if(occluded) {InterlockedAdd(occlusionCounters[0],1);return;}
    if(id==0 && occlusionSettings.w==0)occlusionCounters[3]=1;
    if(software_cluster(cluster,bounds)) {
        uint slot;InterlockedAdd(occlusionEarlySoftware[0],1,slot);occlusionEarlySoftware[slot+1]=id;
    } else {
        uint slot;InterlockedAdd(occlusionEarlyHardware[0],1,slot);occlusionEarlyHardware[slot+1]=id;
    }
}
[shader("compute")]
[numthreads(64,1,1)]
void retest_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=occlusion_index(dispatch);
    if(id>=min(occlusionSelectionCounters[0],occlusionSettings.x) || occlusionFlags[id]!=2)return;
    OcclusionCluster cluster=occlusionClusters[occlusionSelected[id].x];
    ClusterBounds bounds=cluster_bounds(cluster);
    bool occluded=hidden_bounds(bounds);
    occlusionFlags[id]=occluded?4:3;InterlockedAdd(occlusionCounters[occluded?2:1],1);
    if(occluded)return;
    if(software_cluster(cluster,bounds)) {
        uint slot;InterlockedAdd(occlusionLateSoftware[0],1,slot);occlusionLateSoftware[slot+1]=id;
    } else {
        uint slot;InterlockedAdd(occlusionLateHardware[0],1,slot);occlusionLateHardware[slot+1]=id;
    }
}
// One thread finalizes packed 12-byte indirect records from the GPU-written
// list heads: early mesh, early software, late mesh, late software.
[shader("compute")]
[numthreads(1,1,1)]
void bin_finalize_main() {
    [unroll]for(uint phase=0;phase<2;++phase) {
        uint software=phase?occlusionLateSoftware[0]:occlusionEarlySoftware[0];
        uint hardware=phase?occlusionLateHardware[0]:occlusionEarlyHardware[0];
        uint mesh=hardware/32+uint(hardware%32!=0);
        occlusionBinArgs[phase*6+0]=min(mesh,65535u);
        occlusionBinArgs[phase*6+1]=max(1u,mesh/65535+uint(mesh%65535!=0));occlusionBinArgs[phase*6+2]=1;
        occlusionBinArgs[phase*6+3]=min(software,65535u);
        occlusionBinArgs[phase*6+4]=max(1u,software/65535+uint(software%65535!=0));occlusionBinArgs[phase*6+5]=1;
    }
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
