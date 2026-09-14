#ifndef OCTARYN_DDGI_SAMPLE
#define OCTARYN_DDGI_SAMPLE
#include "DDGITypes.slang"
StructuredBuffer<DDGIControl> ddgiControls;
StructuredBuffer<DDGIProbe> ddgiProbes;
StructuredBuffer<float4> ddgiIrradiance;
StructuredBuffer<float2> ddgiDistance;
StructuredBuffer<DDGIControl> ddgiFineControls;
StructuredBuffer<DDGIProbe> ddgiFineProbes;
StructuredBuffer<float4> ddgiFineIrradiance;
StructuredBuffer<float2> ddgiFineDistance;
uniform uint4 ddgiFineGrid;
uniform int4 ddgiFineOrigin;
uniform float4 ddgiFineFadeOrigin;
uniform float4 ddgiFineParameters;
uniform uint4 ddgiFineFrame;
float4 ddgi_read_irradiance(uint probe,float3 direction,uint n,StructuredBuffer<float4> ddgiIrradiance) {
    float2 coordinate=ddgi_oct_encode(direction)*n-.5;
    int2 lo=int2(floor(coordinate));float2 f=frac(coordinate);uint base=probe*n*n;
    return lerp(lerp(ddgiIrradiance[base+ddgi_texel(lo,n)],ddgiIrradiance[base+ddgi_texel(lo+int2(1,0),n)],f.x),
        lerp(ddgiIrradiance[base+ddgi_texel(lo+int2(0,1),n)],ddgiIrradiance[base+ddgi_texel(lo+1,n)],f.x),f.y);
}
float2 ddgi_read_distance(uint probe,float3 direction,uint n,StructuredBuffer<float2> ddgiDistance) {
    float2 coordinate=ddgi_oct_encode(direction)*n-.5;
    int2 lo=int2(floor(coordinate));float2 f=frac(coordinate);uint base=probe*n*n;
    return lerp(lerp(ddgiDistance[base+ddgi_texel(lo,n)],ddgiDistance[base+ddgi_texel(lo+int2(1,0),n)],f.x),
        lerp(ddgiDistance[base+ddgi_texel(lo+int2(0,1),n)],ddgiDistance[base+ddgi_texel(lo+1,n)],f.x),f.y);
}
float4 ddgi_read_irradiance(uint probe,float3 direction) {return ddgi_read_irradiance(probe,direction,ddgiFrame.z,ddgiIrradiance);}
float2 ddgi_read_distance(uint probe,float3 direction) {return ddgi_read_distance(probe,direction,ddgiFrame.w,ddgiDistance);}
float3 ddgi_sample_volume(float3 position,float3 normal,out float coverage,
    uint4 ddgiGrid,int4 ddgiOrigin,float4 ddgiFadeOrigin,float4 ddgiParameters,uint4 ddgiFrame,
    StructuredBuffer<DDGIControl> ddgiControls,StructuredBuffer<DDGIProbe> ddgiProbes,
    StructuredBuffer<float4> ddgiIrradiance,StructuredBuffer<float2> ddgiDistance) {
    coverage=0;if(ddgiGrid.w==0)return 0;
    float spacing=ddgiParameters.x;float3 biased=position+normal*ddgiParameters.w;
    float anchor=float(ddgiOrigin.w)*.5;
    float3 coordinate=biased/spacing-anchor;
    int3 base=int3(floor(coordinate));float3 alpha=frac(coordinate);
    float3 span=float3(max(int3(ddgiGrid.xyz)-2,int3(1)));
    float3 boundary=min(coordinate-ddgiFadeOrigin.xyz,ddgiFadeOrigin.xyz+span-coordinate);
    // Blend over two cells with zero slope at either end. The default eight-cell
    // vertical grid still has a fully weighted interior. The guard cell keeps
    // fading inside valid support without snapping when the toroidal grid moves.
    float fade=smoothstep(0,2,min(boundary.x,min(boundary.y,boundary.z)));
    if(fade<=0)return 0;
    // Known occlusion remains dark. Pending cells can use the coarser field;
    // only missing hierarchy coverage reaches the visibility-tested sky path.
    float3 result=0;float total=0,resolvedSupport=0;
    [unroll]for(uint corner=0;corner<8;++corner) {
        int3 bit=int3(corner&1,(corner>>1)&1,(corner>>2)&1),cell=base+bit;
        if(any(cell<ddgiOrigin.xyz)||any(cell>=ddgiOrigin.xyz+int3(ddgiGrid.xyz)))continue;
        uint index=ddgi_index(cell,ddgiGrid);DDGIControl control=ddgiControls[index];DDGIProbe probe=ddgiProbes[index];
        if(any(control.cell!=cell)||control.version!=probe.metadata.x)continue;
        float3 weights=lerp(1-alpha,alpha,float3(bit));float tri=weights.x*weights.y*weights.z;
        // A newly scrolled or relocating cell has no estimate yet. Hand over
        // gradually; confirmed solid cells remain covered and do not leak sky.
        if(probe.metadata.w!=0||probe.offset.w==1)resolvedSupport+=tri;
        if(probe.metadata.w==0||probe.offset.w==1)continue;
        float3 delta=biased-((float3(cell)+anchor)*spacing+probe.offset.xyz);float distance=length(delta);
        float3 direction=delta/max(distance,1e-5);
        float2 moments=ddgi_read_distance(index,direction,ddgiFrame.w,ddgiDistance);
        float variance=max(moments.y-moments.x*moments.x,.0001);
        float difference=max(distance-moments.x,0);
        float visibility=variance/(variance+difference*difference);
        visibility=pow(visibility,3);
        if(visibility<.001)continue;
        float normalWeight=pow(max(.05,(dot(-direction,normal)+1)*.5),2);
        float weight=normalWeight*visibility;
        // JCGT irradiance fields: suppress small weights to limit wall leaking.
        if(weight<.2)weight*=weight*weight/.04;
        // Apply interpolation after visibility crushing to preserve smooth cells.
        weight*=max(tri,1e-6);
        result+=ddgi_read_irradiance(index,normal,ddgiFrame.z,ddgiIrradiance).rgb*weight;total+=weight;
    }
    // Exact full support avoids launching sky rays for floating-point residue.
    coverage=fade*(resolvedSupport>.99999?1:saturate(resolvedSupport));
    if(total<=1e-10)return 0;
    return max(result/total,0);
}
float3 ddgi_sample(float3 position,float3 normal,out float coverage) {
    float firstCoverage,secondCoverage;
    float3 first=ddgi_sample_volume(position,normal,firstCoverage,ddgiGrid,ddgiOrigin,ddgiFadeOrigin,
        ddgiParameters,ddgiFrame,ddgiControls,ddgiProbes,ddgiIrradiance,ddgiDistance);
    float3 second=ddgi_sample_volume(position,normal,secondCoverage,ddgiFineGrid,ddgiFineOrigin,ddgiFineFadeOrigin,
        ddgiFineParameters,ddgiFineFrame,ddgiFineControls,ddgiFineProbes,ddgiFineIrradiance,ddgiFineDistance);
    // Traces bind their own volume first; final shading binds coarse first.
    // Density ordering makes every consumer query the identical light field.
    bool firstDense=ddgiParameters.x<ddgiFineParameters.x||ddgiFineGrid.w==0;
    float denseCoverage=firstDense?firstCoverage:secondCoverage;
    float sparseCoverage=firstDense?secondCoverage:firstCoverage;
    float3 dense=firstDense?first:second,sparse=firstDense?second:first;
    coverage=denseCoverage+(1-denseCoverage)*sparseCoverage;
    return (dense*denseCoverage+sparse*((1-denseCoverage)*sparseCoverage))/max(coverage,1e-6);
}
float3 ddgi_debug(float3 position,float3 normal,uint mode) {
    if(ddgiGrid.w==0)return float3(.15,0,.15);
    int3 cell=int3(round(position/ddgiParameters.x-float(ddgiOrigin.w)*.5));
    if(any(cell<ddgiOrigin.xyz)||any(cell>=ddgiOrigin.xyz+int3(ddgiGrid.xyz)))return float3(0);
    uint index=ddgi_index(cell);DDGIProbe probe=ddgiProbes[index];
    bool valid=all(ddgiControls[index].cell==cell)&&probe.metadata.x==ddgiControls[index].version;
    if(!valid)return float3(1,0,1);
    if(mode==1) {
        float3 irradiance=ddgi_read_irradiance(index,normal).rgb;return irradiance/(1+irradiance);
    }
    float3 delta=position-((float3(cell)+float(ddgiOrigin.w)*.5)*ddgiParameters.x+probe.offset.xyz);
    float3 direction=delta/max(length(delta),1e-5);
    if(mode==2) {
        float2 moments=ddgi_read_distance(index,direction);
        return float3(moments.x/ddgiParameters.z,sqrt(max(moments.y-moments.x*moments.x,0))/ddgiParameters.z,0);
    }
    if(mode==3)return probe.offset.w==1?float3(1,.1,.1):
        (probe.offset.w==2?float3(.1,.3,1):(probe.metadata.w!=0?float3(.1,1,.2):float3(1,.7,0)));
    if(mode==4)return .5+probe.offset.xyz/(ddgiParameters.x*.9);
    if(mode==5) {
        float age=saturate(float(ddgiFrame.x-probe.metadata.y)/240.0);return float3(age,1-age,0);
    }
    // Grid-cell colors and relocated probe centers on the visible surface.
    float3 cellColor=frac(float3(cell)*float3(.618,.754,.569));
    float marker=1-smoothstep(ddgiParameters.x*.08,ddgiParameters.x*.13,length(delta));
    return lerp(cellColor*.4,float3(1),marker);
}
#endif
