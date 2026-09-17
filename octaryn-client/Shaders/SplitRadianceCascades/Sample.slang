#ifndef OCTARYN_SRC_SAMPLE
#define OCTARYN_SRC_SAMPLE
#include "Resources.slang"
float4 src_read(uint address,bool previous) {return previous?srcPrevious[address]:srcRadiance[address];}
float4 src_angular(uint probe,uint cascade,float3 direction,bool previous) {
    SrcCascade c=srcCascades[cascade];float2 grid=src_encode(direction)*float2(2*c.resolution,c.resolution)-.5;
    int2 base=int2(floor(grid));float2 f=frac(grid);float4 result=0;float weight=0;
    for(uint i=0;i<4;++i) {
        int2 o=int2(i&1,i>>1),q=base+o;
        q.x=(q.x+int(2*c.resolution))%int(2*c.resolution);q.y=clamp(q.y,0,int(c.resolution)-1);
        float w=(o.x!=0?f.x:1-f.x)*(o.y!=0?f.y:1-f.y);
        float4 value=src_read(src_address(probe,cascade,uint(q.y)*2*c.resolution+uint(q.x)),previous);
        if(value.w<0||!all(isfinite(value)))continue;result+=value*w;weight+=w;
    }
    return weight>0?result/weight:float4(0,0,0,-1);
}
float4 src_sample_direction(float3 position,uint lod,float3 direction,bool previous) {
    int3 base;float3 f;src_spatial(position,lod,0,base,f);float4 sum=0;float weight=0;
    for(uint i=0;i<8;++i) {
        int3 o=int3(i&1,(i>>1)&1,(i>>2)&1);uint p=src_find(base+o,lod,0);if(p==SRC_INVALID)continue;
        float3 t=lerp(1-f,f,float3(o));float w=t.x*t.y*t.z*max(srcProbes[p].quality,.001);
        float4 v=src_angular(p,0,direction,previous);if(v.w<0)continue;sum+=v*w;weight+=w;
    }
    return weight>0?sum/weight:float4(0,0,0,-1);
}
#endif
