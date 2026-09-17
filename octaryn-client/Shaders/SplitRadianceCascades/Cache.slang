#include "Resources.slang"
// Claimed slots are never spun on: a contending surface is retried next frame.
// Publication and all lookups happen in separate dispatches.
uint src_insert(int3 cell,uint lod,uint cascade,uint lifetime,uint existing,float3 position,float3 normal) {
    SrcCascade c=srcCascades[cascade];uint start=src_key_hash(cell,lod)&(c.hashCapacity-1);
    for(uint k=0;k<min(srcLimits.y,c.hashCapacity);++k) {
        uint address=c.hashOffset+((start+k)&(c.hashCapacity-1)),slot;
        InterlockedCompareExchange(srcHash[address],0,existing==SRC_INVALID?SRC_INVALID:existing+1,slot);
        if(slot==0 && existing!=SRC_INVALID)return existing;
        if(slot==SRC_INVALID) {InterlockedAdd(srcCounters[40],1);return SRC_INVALID;}
        if(slot!=0) {
            SrcProbe p=srcProbes[slot-1];
            if(p.lod==lod && all(p.cell==cell))return slot-1;
            InterlockedAdd(srcCounters[cascade*4+3],1);continue;
        }
        uint index=existing;
        if(index==SRC_INVALID) {
            uint used;InterlockedAdd(srcCounters[cascade*4+1],1,used);
            if(used>=srcCounters[cascade*4]) {
                InterlockedAdd(srcCounters[cascade*4+2],1);srcHash[address]=0;return SRC_INVALID;
            }
            index=srcFreelist[c.probeOffset+used];
            SrcProbe p;p.cell=cell;p.lod=lod;p.lifetime=lifetime;p.epoch=srcExtent.w;p.quality=0;p.reserved=0;
            uint shift=uint(log2(srcIntervals.x))+lod+cascade;
            p.anchor=float4(position-float3((cell<<shift)-srcOrigin.xyz),1);
            p.normal=float4(normal,0);
            srcProbes[index]=p;
        }
        DeviceMemoryBarrier();InterlockedExchange(srcHash[address],index+1);return index;
    }
    InterlockedAdd(srcCounters[cascade*4+2],1);return SRC_INVALID;
}
[shader("compute")][numthreads(64,1,1)]
void reset(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    if(id.x<srcCounts.w)srcHash[id.x]=0;
    if(id.x<96)srcCounters[id.x]=0;
}
[shader("compute")][numthreads(64,1,1)]
void maintain(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    if(id.x>=srcCounts.y)return;SrcProbe p=srcProbes[id.x];
    srcDistribution[id.x]=0;
    if(p.epoch!=srcExtent.w)p.lifetime=0;
    else if(p.lifetime>0)--p.lifetime;
    p.quality*=srcTemporal.x;srcProbes[id.x]=p;
    if(p.lifetime==0) {
        uint c=src_probe_cascade(id.x),free;InterlockedAdd(srcCounters[c*4],1,free);
        srcFreelist[srcCascades[c].probeOffset+free]=id.x;
    }
}
[shader("compute")][numthreads(64,1,1)]
void decay(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    if(id.x>=srcCounts.z)return;uint c=0;
    for(uint i=1;i<srcCounts.x;++i)if(id.x>=srcCascades[i].directionOffset)c=i;
    SrcCascade level=srcCascades[c];uint probe=level.probeOffset+(id.x-level.directionOffset)/level.directions;
    bool alive=srcProbes[probe].lifetime>0;
    srcFluence[id.x]=alive?uint4(float4(srcFluence[id.x])*srcTemporal.x):uint4(0,0,0,0);
    srcWeights[id.x]=alive?uint(float(srcWeights[id.x])*srcTemporal.x):0u;
    if(!alive)srcRadiance[id.x]=float4(0,0,0,-1);
}
[shader("compute")][numthreads(64,1,1)]
void rehash(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    if(id.x>=srcCounts.y)return;SrcProbe p=srcProbes[id.x];if(p.lifetime==0)return;
    src_insert(p.cell,p.lod,src_probe_cascade(id.x),p.lifetime,id.x,0,0);
}
[shader("compute")][numthreads(64,1,1)]
void visible(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    if(id.x>=srcExtent.x*srcExtent.y)return;float3 position,normal;if(!src_surface(uint2(id.x%srcExtent.x,id.x/srcExtent.x),position,normal))return;
    uint lod=src_lod(position);
    src_insert(src_cell(position,lod,0),lod,0,srcLimits.z,SRC_INVALID,position,normal);
    if(src_lod_weight(position,lod)<1)src_insert(src_cell(position,lod+1,0),lod+1,0,srcLimits.z,SRC_INVALID,position,normal);
}
[shader("compute")][numthreads(64,1,1)]
void secondary(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    if(id.x>=src_primary_count())return;SrcRay ray=srcRays[id.x];if(ray.hitPosition.w==0)return;
    uint lod=src_lod(ray.hitPosition.xyz)+2;
    src_insert(src_cell(ray.hitPosition.xyz,lod,0),lod,0,srcLimits.w,SRC_INVALID,ray.hitPosition.xyz,ray.hitNormal.xyz);
}
[shader("compute")][numthreads(64,1,1)]
void parents(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    SrcCascade c=srcCascades[srcLevel];if(id.x>=c.capacity)return;
    SrcProbe p=srcProbes[c.probeOffset+id.x];if(p.lifetime==0)return;
    src_insert(p.cell>>1,p.lod,srcLevel+1,p.lifetime,SRC_INVALID,src_anchor(p,srcLevel),p.normal.xyz);
}
[shader("compute")][numthreads(64,1,1)]
void refresh(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    if(id.x>=(srcStage==0?srcExtent.x*srcExtent.y:src_primary_count()))return;float3 p,n;uint lod,life;
    if(srcStage==0) {if(!src_surface(uint2(id.x%srcExtent.x,id.x/srcExtent.x),p,n))return;lod=src_lod(p);life=srcLimits.z;}
    else {SrcRay ray=srcRays[id.x];if(ray.hitPosition.w==0)return;p=ray.hitPosition.xyz;n=ray.hitNormal.xyz;lod=src_lod(p)+2;life=srcLimits.w;}
    uint lodCount=srcStage==0 && src_lod_weight(p,lod)<1?2:1;
    for(uint l=0;l<lodCount;++l)for(uint c=0;c<srcCounts.x;++c) {
        uint index=src_find(src_cell(p,lod+l,c),lod+l,c);
        if(index!=SRC_INVALID) {
            InterlockedMax(srcProbes[index].lifetime,life);
            uint previous;InterlockedCompareExchange(srcDistribution[index].w,0,1,previous);
            if(previous==0) {
                SrcProbe probe=srcProbes[index];uint shift=uint(log2(srcIntervals.x))+probe.lod+c;
                srcProbes[index].anchor=float4(p-float3((probe.cell<<shift)-srcOrigin.xyz),1);
                srcProbes[index].normal=float4(n,0);
            }
        }
    }
}
