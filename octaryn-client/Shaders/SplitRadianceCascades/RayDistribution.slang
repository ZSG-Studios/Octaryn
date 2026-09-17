#include "Resources.slang"
[shader("compute")][numthreads(64,1,1)]
void count(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);if(id.x>=src_primary_count())return;
    float3 position,normal;if(!src_surface(src_pixel(id.x),position,normal))return;
    uint lod=src_lod(position),probe=src_find(src_cell(position,lod,0),lod,0);
    if(probe!=SRC_INVALID)InterlockedAdd(srcDistribution[probe].x,1);
}
[shader("compute")][numthreads(64,1,1)]
void propagate(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);SrcCascade c=srcCascades[srcLevel];if(id.x>=c.capacity)return;
    uint index=c.probeOffset+id.x;SrcProbe p=srcProbes[index];if(p.lifetime==0)return;
    uint parent=src_find(p.cell>>1,p.lod,srcLevel+1);
    if(parent!=SRC_INVALID)InterlockedAdd(srcDistribution[parent].x,srcDistribution[index].x);
}
[shader("compute")][numthreads(64,1,1)]
void offsets(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);SrcCascade c=srcCascades[srcLevel];if(id.x>=c.capacity)return;
    uint index=c.probeOffset+id.x;SrcProbe p=srcProbes[index];if(p.lifetime==0)return;
    if(srcLevel==srcCounts.x-1) {
        uint offset;InterlockedAdd(srcCounters[48],srcDistribution[index].x,offset);srcDistribution[index].y=offset;
    }
    // Each child has precisely one parent. Parent-local contiguous ranges make
    // one R2 sequence cover all visible samples in that parent's angular domain.
    if(srcLevel==0)return;uint offset=srcDistribution[index].y;
    for(uint i=0;i<8;++i) {
        int3 o=int3(i&1,(i>>1)&1,(i>>2)&1);uint child=src_find((p.cell<<1)+o,p.lod,srcLevel-1);
        if(child==SRC_INVALID)continue;srcDistribution[child].y=offset;offset+=srcDistribution[child].x;
    }
}
