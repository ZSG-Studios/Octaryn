// GeometryFormat/Selection.h are the CPU layout owners. Dispatch reset, then
// select once per depth descending with UAV barriers, then compact and finish.
struct SelectionGroup {
    float3 center;float radius;float error;
    uint firstPage;uint pageCount;uint firstParent;uint parentCount;uint depth;uint root;uint reserved;
};
struct SelectionCluster {uint group;uint refinedGroup;uint page;uint reserved;float3 center;float radius;};
StructuredBuffer<SelectionGroup> geometryGroups;
StructuredBuffer<SelectionCluster> geometryClusters;
StructuredBuffer<uint> geometryGroupPages;
StructuredBuffer<uint> geometryParents;
// slot, generation, resident, reserved. Table is immutable for this submission.
StructuredBuffer<uint4> geometryPages;
RWStructuredBuffer<uint> geometryActiveGroups;
RWStructuredBuffer<uint> geometryRequestedPages;
RWStructuredBuffer<uint> geometryRequestPriorities;
RWStructuredBuffer<uint2> geometryFeedback;
// cluster, page, physical slot, generation.
RWStructuredBuffer<uint4> geometrySelected;
// selected count, feedback count, feedback overflow, selected overflow, missing roots.
RWStructuredBuffer<uint> geometryCounters;
RWStructuredBuffer<uint> geometryDispatch;
uniform uint4 geometryCounts; // groups, pages, clusters, current depth
uniform uint2 geometryLimits; // selected cluster capacity, feedback capacity
uniform float4 geometryEyeFocal;
uniform float geometryErrorPixels;
uniform float4 geometryPlanes[6];
uniform uint geometryFrustum;
uint selection_index(uint3 dispatch) {return dispatch.x+dispatch.y*(65535u*64u);}
bool sphere_visible(float3 center,float radius) {
    if(geometryFrustum!=0)for(uint i=0;i<6;++i) {
        float4 plane=geometryPlanes[i];
        if(dot(plane.xyz,center)+plane.w < -radius*length(plane.xyz))return false;
    }
    return true;
}

bool reserve_slot(uint counter,uint capacity,out uint slot) {
    slot=geometryCounters[counter];
    while(slot<capacity) {
        uint previous;
        InterlockedCompareExchange(geometryCounters[counter],slot,slot+1,previous);
        if(previous==slot)return true;
        slot=previous;
    }
    return false;
}
void request_page(uint page,float priority) {
    InterlockedMax(geometryRequestPriorities[page],asuint(priority));
    uint previous;InterlockedExchange(geometryRequestedPages[page],1,previous);
    if(previous!=0)return;
    uint slot;
    if(reserve_slot(1,geometryLimits.y,slot))geometryFeedback[slot]=uint2(page,0);
    else InterlockedOr(geometryCounters[2],1);
}
[shader("compute")]
[numthreads(64,1,1)]
void reset_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=selection_index(dispatch);
    if(id<geometryCounts.x)geometryActiveGroups[id]=0;
    if(id<geometryCounts.y) {geometryRequestedPages[id]=0;geometryRequestPriorities[id]=0;}
    if(id<5)geometryCounters[id]=0;
    if(id<6)geometryDispatch[id]=id%3==0?0:1;
}
[shader("compute")]
[numthreads(64,1,1)]
void select_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=selection_index(dispatch);if(id>=geometryCounts.x)return;
    SelectionGroup group=geometryGroups[id];if(group.depth!=geometryCounts.w)return;
    bool parentsActive=true;
    for(uint p=0;p<group.parentCount;++p)
        parentsActive=parentsActive && geometryActiveGroups[geometryParents[group.firstParent+p]]!=0;
    float distance=max(.001,length(group.center-geometryEyeFocal.xyz)-group.radius);
    float pixels=group.root!=0?3.402823466e+38:min(3.402823466e+38,group.error*geometryEyeFocal.w/distance);
    if(group.root==0 && (!parentsActive || pixels<=geometryErrorPixels))return;
    if(group.root==0 && !sphere_visible(group.center,group.radius))return;
    bool complete=true;
    for(uint p=0;p<group.pageCount;++p) {
        uint page=geometryGroupPages[group.firstPage+p];uint4 entry=geometryPages[page];
        if(entry.z==0 || entry.x==0xffffffff || entry.y==0) {complete=false;request_page(page,pixels);}
    }
    if(group.root!=0 && !complete)InterlockedOr(geometryCounters[4],1);
    geometryActiveGroups[id]=complete?1:0;
}
[shader("compute")]
[numthreads(64,1,1)]
void compact_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=selection_index(dispatch);if(id>=geometryCounts.z)return;
    SelectionCluster cluster=geometryClusters[id];
    if(geometryActiveGroups[cluster.group]==0)return;
    if(cluster.refinedGroup!=0xffffffff && geometryActiveGroups[cluster.refinedGroup]!=0)return;
    if(!sphere_visible(cluster.center,cluster.radius))return;
    uint slot;InterlockedAdd(geometryCounters[0],1,slot);
    if(slot>=geometryLimits.x) {InterlockedOr(geometryCounters[3],1);return;}
    uint4 page=geometryPages[cluster.page];geometrySelected[slot]=uint4(id,cluster.page,page.x,page.y);
}
[shader("compute")]
[numthreads(1,1,1)]
void finish_main() {
    // No partial cut can silently reach rendering after overflow or a missing root.
    bool valid=geometryCounters[3]==0 && geometryCounters[4]==0;
    uint meshGroups=valid?geometryCounters[0]/32+uint(geometryCounters[0]%32!=0):0;
    uint softwareGroups=valid?geometryCounters[0]:0;
    geometryDispatch[0]=min(meshGroups,65535u);
    geometryDispatch[1]=max(1u,meshGroups/65535+uint(meshGroups%65535!=0));geometryDispatch[2]=1;
    geometryDispatch[3]=min(softwareGroups,65535u);
    geometryDispatch[4]=max(1u,softwareGroups/65535+uint(softwareGroups%65535!=0));geometryDispatch[5]=1;
}
[shader("compute")]
[numthreads(64,1,1)]
void feedback_main(uint3 dispatch:SV_DispatchThreadID) {
    uint id=selection_index(dispatch);if(id>=geometryCounters[1])return;
    uint page=geometryFeedback[id].x;
    geometryFeedback[id]=uint2(page,geometryRequestPriorities[page]);
}
