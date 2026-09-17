#include "DDGITypes.slang"
StructuredBuffer<DDGIControl> ddgiControls;
RWStructuredBuffer<DDGIProbe> ddgiProbes;
RWStructuredBuffer<float4> ddgiIrradiance;
RWStructuredBuffer<float2> ddgiDistance;
RWStructuredBuffer<float> ddgiVariability;
uniform uint ddgiProbeCount;
float2 ddgi_seed_distance(uint probe,float3 direction) {
    uint n=ddgiFrame.w;float2 coordinate=ddgi_oct_encode(direction)*n-.5;
    int2 lo=int2(floor(coordinate));float2 f=frac(coordinate);uint base=probe*n*n;
    return lerp(lerp(ddgiDistance[base+ddgi_texel(lo,n)],ddgiDistance[base+ddgi_texel(lo+int2(1,0),n)],f.x),
        lerp(ddgiDistance[base+ddgi_texel(lo+int2(0,1),n)],ddgiDistance[base+ddgi_texel(lo+1,n)],f.x),f.y);
}
bool ddgi_seed_source(int3 cell,float3 target,out uint index) {
    index=ddgi_index(cell);
    DDGIControl control=ddgiControls[index];DDGIProbe probe=ddgiProbes[index];
    // Only already-stable world cells. A wrapped slot's control.cell will not
    // match, so toroidal neighbors cannot donate the trailing-edge field.
    if(!all(control.cell==cell)||control.padding.x!=0||probe.metadata.x!=control.version||
        probe.metadata.w==0||probe.offset.w==1||probe.metadata.y==0||probe.metadata.y>=ddgiFrame.x||
        control.padding.y!=0||(control.padding.z==2&&control.refreshFrame>probe.metadata.y))return false;
    // Donate only what the donor itself can see. Reject solely when its stored
    // visibility confidently ends in a wall before the target: mean distance
    // plus two standard deviations and half a cell still falls short. Noisy
    // one-block and cave moments always pass instead of starving the seed.
    float3 donor=(float3(cell)+float(ddgiOrigin.w)*.5)*ddgiParameters.x+probe.offset.xyz;
    float3 delta=target-donor;float distance=length(delta);
    if(distance<1e-4)return true;
    float2 moments=ddgi_seed_distance(index,delta/distance);
    float stddev=sqrt(max(moments.y-moments.x*moments.x,0));
    return distance<=moments.x+2*stddev+ddgiParameters.x*.5;
}
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=ddgiProbeCount)return;
    DDGIControl control=ddgiControls[id.x];DDGIProbe probe=ddgiProbes[id.x];
    bool opened=control.padding.y!=0;
    bool scrolled=probe.metadata.x!=control.version;
    if(control.padding.x!=0||!(scrolled||opened||probe.metadata.y==0))return;
    uint n=ddgiFrame.z,d=ddgiFrame.w,sources=0;
    uint neighbors[6];
    const int3 step[6]={int3(1,0,0),int3(-1,0,0),int3(0,1,0),int3(0,-1,0),int3(0,0,1),int3(0,0,-1)};
    uint maxWalk=max(ddgiGrid.x,max(ddgiGrid.y,ddgiGrid.z));
    float3 target=(float3(control.cell)+float(ddgiOrigin.w)*.5)*ddgiParameters.x;
    // Never borrow through a known wall or from another seed in this dispatch.
    [unroll]for(uint i=0;i<6;++i) {
        for(uint dist=1;dist<=maxWalk;++dist) {
            uint index;
            if(!ddgi_seed_source(control.cell+step[i]*int(dist),target,index))continue;
            neighbors[sources++]=index;break;
        }
    }
    if(sources==0) {
        if(scrolled) {
            // Pending with a matched version: the gather leaves this corner
            // to the coarser field instead of sampling the wrapped slot.
            probe=(DDGIProbe)0;probe.metadata.x=control.version;
            ddgiProbes[id.x]=probe;
        }
        return;
    }
    uint irradianceCount=n*n,distanceCount=d*d;
    for(uint texel=0;texel<irradianceCount;++texel) {
        float3 sum=0;
        for(uint i=0;i<sources;++i)sum+=ddgiIrradiance[neighbors[i]*irradianceCount+texel].rgb;
        ddgiIrradiance[id.x*irradianceCount+texel]=float4(sum/float(sources),1);
    }
    for(uint texel=0;texel<distanceCount;++texel) {
        float2 sum=0;
        for(uint i=0;i<sources;++i)sum+=ddgiDistance[neighbors[i]*distanceCount+texel];
        ddgiDistance[id.x*distanceCount+texel]=sum/float(sources);
    }
    // Seeded values are provisional, never a completed trace/history sample.
    probe.offset=0;probe.metadata=uint4(control.version,0,0,1);
    ddgiProbes[id.x]=probe;ddgiVariability[id.x]=0;
}
