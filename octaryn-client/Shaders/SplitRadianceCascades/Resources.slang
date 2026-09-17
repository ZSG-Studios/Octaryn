#ifndef OCTARYN_SRC_RESOURCES
#define OCTARYN_SRC_RESOURCES
#include "../Lighting/Surface.slang"
struct SrcCascade {uint probeOffset,capacity,directionOffset,directions,hashOffset,hashCapacity,resolution,padding;};
struct SrcProbe {int3 cell;uint lod;uint lifetime,epoch;float quality;uint reserved;float4 anchor;float4 normal;};
struct SrcLinks {uint4 indices[2];float4 weights[2];};
struct SrcRay {float4 origin,direction,radiance,hitPosition,hitNormal;};
uniform SrcCascade srcCascades[8];
uniform uint4 srcExtent; // width, height, frame, epoch
uniform uint4 srcCounts; // cascades, probes, directions, hash entries
uniform uint4 srcLimits; // ray capacity, hash search bound, visible life, secondary life
uniform float4 srcIntervals; // spacing, contact length, base interval, growth
uniform float4 srcTemporal; // decay, feedback, trace limit, unused
uniform float4 srcLod; // radius, transition ratio
uniform float4 srcEye;
uniform int4 srcOrigin;
uniform uint srcLevel;
uniform uint srcStage;
RWStructuredBuffer<SrcProbe> srcProbes;
RWStructuredBuffer<uint> srcHash,srcFreelist,srcCounters;
RWStructuredBuffer<uint4> srcFluence;
RWStructuredBuffer<uint> srcWeights;
RWStructuredBuffer<float4> srcRadiance;
RWStructuredBuffer<float4> srcPrevious; // UAV-only keeps scratch buffers barrier-free.
RWStructuredBuffer<SrcRay> srcRays;
RWStructuredBuffer<SrcLinks> srcLinks;
RWStructuredBuffer<uint4> srcDistribution; // count, hierarchical offset, cursor, reserved
Texture2D<float4> srcPositions;
Texture2D<float4> srcNormals;
RWTexture2D<float4> srcLighting;
RWStructuredBuffer<float4> srcOct; // 36 cosine-weighted irradiance bins per probe.
RWStructuredBuffer<float4> srcOctPrev; // Last frame's bins, used for multi-bounce feedback.
static const uint SRC_INVALID=0xffffffffu;
static const float SRC_PI=3.141592653589793;
uint src_index(uint3 id) {return id.x+id.y*4194240u;}
uint src_hash(uint x) {x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
uint src_key_hash(int3 p,uint lod) {return src_hash(uint(p.x)^src_hash(uint(p.y))^src_hash(uint(p.z)+lod*0x9e3779b9u));}
float src_random(inout uint rng) {rng=src_hash(rng+0x9e3779b9u);return float(rng>>8)*(1.0/16777216.0);}
uint src_primary_count() {return min(srcExtent.x*srcExtent.y,srcLimits.x/2);}
uint2 src_pixel(uint i) {
    uint count=src_primary_count(),pixels=srcExtent.x*srcExtent.y;
    // Stable stratification across the whole viewport, rotated each frame.
    uint index=i*(pixels/max(count,1u))+uint(float(i)*float(pixels%max(count,1u))/float(max(count,1u)));
    uint stride=max(pixels/max(count,1u),1u);
    index=min(index+srcExtent.z%stride,pixels-1);
    return uint2(index%srcExtent.x,index/srcExtent.x);
}
bool src_surface(uint2 pixel,out float3 position,out float3 normal) {
    float4 p=srcPositions.Load(int3(pixel,0));float4 n=srcNormals.Load(int3(pixel,0));
    position=p.xyz+srcEye.xyz;uint voxel=lighting_voxel(n);normal=lighting_normal(voxel);
    if((voxel&256)!=0&&dot(normal,-p.xyz)<0)normal=-normal;
    return p.w>0 && all(isfinite(position));
}
uint src_lod(float3 p) {
    float radius=max(max(abs(p.x-srcEye.x),abs(p.y-srcEye.y)),abs(p.z-srcEye.z));
    return uint(clamp(ceil(log2(max(radius/srcLod.x,1.0))),0.0,18.0));
}
float src_lod_weight(float3 p,uint lod) {
    float radius=max(max(abs(p.x-srcEye.x),abs(p.y-srcEye.y)),abs(p.z-srcEye.z));
    float boundary=srcLod.x*exp2(float(lod));
    return saturate((boundary-radius)/(boundary*(1-1/srcLod.y)));
}
int3 src_cell(float3 p,uint lod,uint cascade) {
    uint shift=uint(log2(srcIntervals.x))+lod+cascade;
    return (srcOrigin.xyz+int3(floor(p)))>>shift;
}
float3 src_anchor(SrcProbe p,uint cascade) {
    uint shift=uint(log2(srcIntervals.x))+p.lod+cascade;
    return float3((p.cell<<shift)-srcOrigin.xyz)+p.anchor.xyz;
}
void src_spatial(float3 p,uint lod,uint cascade,out int3 base,out float3 f) {
    uint shift=uint(log2(srcIntervals.x))+lod+cascade;
    int mask=int((1u<<shift)-1u);
    float3 local=(float3(srcOrigin.xyz&mask)+p)*exp2(-float(shift))-.5;
    base=(srcOrigin.xyz>>shift)+int3(floor(local));f=frac(local);
}
uint src_probe_cascade(uint index) {uint c=0;for(uint i=1;i<srcCounts.x;++i)if(index>=srcCascades[i].probeOffset)c=i;return c;}
uint src_address(uint probe,uint cascade,uint dir) {
    SrcCascade c=srcCascades[cascade];return c.directionOffset+(probe-c.probeOffset)*c.directions+dir;
}
uint src_find(int3 cell,uint lod,uint cascade) {
    SrcCascade c=srcCascades[cascade];uint start=src_key_hash(cell,lod)&(c.hashCapacity-1);
    for(uint k=0;k<min(srcLimits.y,c.hashCapacity);++k) {
        uint slot=srcHash[c.hashOffset+((start+k)&(c.hashCapacity-1))];
        if(slot==0)return SRC_INVALID;
        if(slot==SRC_INVALID)continue;
        SrcProbe p=srcProbes[slot-1];if(p.lifetime>0 && p.lod==lod && all(p.cell==cell))return slot-1;
    }
    return SRC_INVALID;
}
float2 src_encode(float3 dir) {return float2(frac(atan2(dir.z,dir.x)/(2*SRC_PI)+1),saturate(dir.y*.5+.5));}
float3 src_decode(float2 uv) {float y=uv.y*2-1,a=uv.x*(2*SRC_PI),r=sqrt(max(0,1-y*y));return float3(cos(a)*r,y,sin(a)*r);}
uint src_direction(float3 direction,uint cascade) {
    uint r=srcCascades[cascade].resolution;uint2 q=min(uint2(src_encode(direction)*float2(2*r,r)),uint2(2*r-1,r-1));
    return q.y*(2*r)+q.x;
}
float3 src_bin_direction(uint bin,uint cascade) {uint r=srcCascades[cascade].resolution;return src_decode((float2(bin%(2*r),bin/(2*r))+.5)/float2(2*r,r));}
uint src_oct_bins() {return 36u;}
float3 src_oct_direction(uint bin) {uint u=bin%6u,v=bin/6u;return src_decode(float2((float(u)+.5)/6.0,(float(v)+.5)/6.0));}
float4 src_oct_sample_prev(uint probe,float3 normal) {
    float2 uv=src_encode(normal)*6.0-.5;
    int2 base=int2(floor(uv));float2 f=frac(uv);float4 sum=0;float weight=0;
    for(uint i=0;i<4;++i) {
        int2 o=int2(i&1,i>>1);int2 q=base+o;
        q.x=(q.x+6)%6;q.y=clamp(q.y,0,5);
        float w=(o.x!=0?f.x:1-f.x)*(o.y!=0?f.y:1-f.y);
        float4 value=srcOctPrev[probe*36u+uint(q.y)*6u+uint(q.x)];
        if(value.w<=0)continue;
        sum+=value*w;weight+=w;
    }
    return weight>0?sum/weight:float4(0,0,0,-1);
}
float4 src_oct_sample(uint probe,float3 normal) {
    float2 uv=src_encode(normal)*6.0-.5;
    int2 base=int2(floor(uv));float2 f=frac(uv);float4 sum=0;float weight=0;
    for(uint i=0;i<4;++i) {
        int2 o=int2(i&1,i>>1);int2 q=base+o;
        q.x=(q.x+6)%6;q.y=clamp(q.y,0,5);
        float w=(o.x!=0?f.x:1-f.x)*(o.y!=0?f.y:1-f.y);
        float4 value=srcOct[probe*36u+uint(q.y)*6u+uint(q.x)];
        if(value.w<=0)continue;
        sum+=value*w;weight+=w;
    }
    return weight>0?sum/weight:float4(0,0,0,-1);
}
#endif
