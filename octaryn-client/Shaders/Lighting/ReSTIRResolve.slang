#ifndef OCTARYN_RESTIR_RESOLVE_SLANG
#define OCTARYN_RESTIR_RESOLVE_SLANG
#include "LocalLighting.slang"
#ifdef OCTARYN_CLUSTERED_LOCAL
#include "ClusteredLocalResolve.slang"
#endif
StructuredBuffer<Reservoir> initial,selected;
RWStructuredBuffer<Reservoir> history;
RWStructuredBuffer<LocalHistory> surfaceHistory;
RWTexture2D<float4> localLighting;
RWStructuredBuffer<uint> localCounters;
groupshared uint localGroupCounters[4];
void count_local_event(uint counter,bool event) {
    if(event)InterlockedAdd(localGroupCounters[counter],1);
}
[shader("compute")][numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID,uint groupIndex:SV_GroupIndex) {
    if(groupIndex<4)localGroupCounters[groupIndex]=0;
    GroupMemoryBarrierWithGroupSync();
    if(all(id.xy<extent.xy)) {
    uint index=id.y*extent.x+id.x;
    Reservoir r=selected[index];LocalSurface s=local_surface(id.xy);
    float3 value=0;float visibility=1;
    if(s.valid>0 && r.M>0 && r.light<extent.w) {
        count_local_event(1,true);
        count_local_event(2,(r.reuse&4)!=0);
        count_local_event(3,(r.reuse&8)!=0);
        float3 L;float distance;
        value=local_contribution(s,r.light,r.sample,L,distance);
#ifdef OCTARYN_RESTIR_RT
        if(any(value>0)) {
            // A range-limited acceleration structure cannot prove distant visibility.
            visibility=world_local_light_visibility(s.position,s.normal,r.light,L,distance);
            count_local_event(0,distance<=raySettings.y);
        }
#endif
#ifdef OCTARYN_CLUSTERED_LOCAL
        visibility=local_shadow_visibility(r.light,s.position,s.normal);
#endif
        value*=reservoir_weight(r)*visibility;
    }
#ifdef OCTARYN_CLUSTERED_LOCAL
    if(s.valid>0)value=clustered_local_lighting(s,id.xy,value);
#endif
    if(restirSettings.w==1)value=float3(restir_hash(r.light)&255,(restir_hash(r.light)>>8)&255,(restir_hash(r.light)>>16)&255)/255.0;
    if(restirSettings.w==2)value=float3(r.age/32.0,0,0);
    if(restirSettings.w==3)value=float3(r.M/max(reuseSettings.w,1));
    if(restirSettings.w==4)value=float3((r.reuse&4)!=0?1:0,0,0);
    if(restirSettings.w==5)value=float3(0,(r.reuse&8)!=0?1:0,0);
    if(restirSettings.w==6)value=float3(visibility);
    if(restirSettings.w==7)value=float3(log2(1+max(r.weightSum,0))/16);
    if(restirSettings.w==8)value=float3(log2(1+extent.w)/16.0);
    localLighting[id.xy]=float4(max(value,0),1);
    // Feed temporal-only history back: spatially correlated streams do not compound.
    history[index]=initial[index];surfaceHistory[index]=local_history(s);
    }
    GroupMemoryBarrierWithGroupSync();
    if(groupIndex<4 && localGroupCounters[groupIndex]>0)InterlockedAdd(localCounters[groupIndex],localGroupCounters[groupIndex]);
}
#endif
