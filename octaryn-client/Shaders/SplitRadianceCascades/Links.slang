#include "Geometry.slang"
[shader("compute")][numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);if(id.x>=srcCounts.y)return;SrcProbe p=srcProbes[id.x];
    if(p.lifetime==0)return;uint cascade=src_probe_cascade(id.x);if(cascade+1>=srcCounts.x)return;
    SrcLinks links;links.indices[0]=links.indices[1]=uint4(SRC_INVALID);links.weights[0]=links.weights[1]=0;
    int3 base=(p.cell-1)>>1;float3 f=.75-float3(p.cell&1)*.5;
    float3 position=src_probe_surface(id.x,cascade);
    for(uint i=0;i<8;++i) {
        int3 o=int3(i&1,(i>>1)&1,(i>>2)&1);uint parent=src_find(base+o,p.lod,cascade+1);
        if(parent==SRC_INVALID)continue;float3 donor=src_probe_surface(parent,cascade+1);
        if(!src_visible(position,donor)) {InterlockedAdd(srcCounters[45],1);continue;}
        float3 t=lerp(1-f,f,float3(o));links.indices[i/4][i%4]=parent;links.weights[i/4][i%4]=t.x*t.y*t.z;
    }
    srcLinks[id.x]=links;
}
