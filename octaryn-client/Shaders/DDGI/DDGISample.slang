#ifndef OCTARYN_DDGI_SAMPLE
#define OCTARYN_DDGI_SAMPLE
#include "DDGITypes.slang"
StructuredBuffer<DDGIControl> ddgiControls;
StructuredBuffer<DDGIProbe> ddgiProbes;
StructuredBuffer<float4> ddgiIrradiance;
StructuredBuffer<float2> ddgiDistance;
float4 ddgi_read_irradiance(uint probe,float3 direction) {
    uint n=ddgiFrame.z;float2 coordinate=ddgi_oct_encode(direction)*n-.5;
    int2 lo=int2(floor(coordinate));float2 f=frac(coordinate);uint base=probe*n*n;
    return lerp(lerp(ddgiIrradiance[base+ddgi_texel(lo,n)],ddgiIrradiance[base+ddgi_texel(lo+int2(1,0),n)],f.x),
        lerp(ddgiIrradiance[base+ddgi_texel(lo+int2(0,1),n)],ddgiIrradiance[base+ddgi_texel(lo+1,n)],f.x),f.y);
}
float2 ddgi_read_distance(uint probe,float3 direction) {
    uint n=ddgiFrame.w;float2 coordinate=ddgi_oct_encode(direction)*n-.5;
    int2 lo=int2(floor(coordinate));float2 f=frac(coordinate);uint base=probe*n*n;
    return lerp(lerp(ddgiDistance[base+ddgi_texel(lo,n)],ddgiDistance[base+ddgi_texel(lo+int2(1,0),n)],f.x),
        lerp(ddgiDistance[base+ddgi_texel(lo+int2(0,1),n)],ddgiDistance[base+ddgi_texel(lo+1,n)],f.x),f.y);
}
float3 ddgi_sample(float3 position,float3 normal,out float coverage) {
    coverage=0;if(ddgiGrid.w==0)return 0;
    float spacing=ddgiParameters.x;float3 coordinate=position/spacing;
    int3 base=int3(floor(coordinate));float3 alpha=frac(coordinate);
    float3 boundary=min(coordinate-float3(ddgiOrigin.xyz),float3(ddgiOrigin.xyz+int3(ddgiGrid.xyz)-1)-coordinate);
    float fade=saturate(min(boundary.x,min(boundary.y,boundary.z)));
    if(fade<=0)return 0;
    float3 biased=position+normal*ddgiParameters.w,result=0;float total=0,validWeight=0;
    [unroll]for(uint corner=0;corner<8;++corner) {
        int3 bit=int3(corner&1,(corner>>1)&1,(corner>>2)&1),cell=base+bit;
        if(any(cell<ddgiOrigin.xyz)||any(cell>=ddgiOrigin.xyz+int3(ddgiGrid.xyz)))continue;
        uint index=ddgi_index(cell);DDGIControl control=ddgiControls[index];DDGIProbe probe=ddgiProbes[index];
        if(any(control.cell!=cell)||control.version!=probe.metadata.x||probe.metadata.w==0||probe.offset.w==1)continue;
        float3 delta=biased-(float3(cell)*spacing+probe.offset.xyz);float distance=length(delta);
        float3 direction=delta/max(distance,1e-5);
        float3 weights=lerp(1-alpha,alpha,float3(bit));float tri=weights.x*weights.y*weights.z;
        float2 moments=ddgi_read_distance(index,direction);
        float variance=max(moments.y-moments.x*moments.x,.0001);
        float difference=max(distance-moments.x,0);
        float visibility=variance/(variance+difference*difference);
        visibility=max(pow(visibility,3),.0001);
        float normalWeight=pow(max(.05,(dot(-direction,normal)+1)*.5),2);
        float weight=max(tri,1e-6)*normalWeight*visibility;
        // JCGT irradiance fields: suppress small weights to limit wall leaking.
        if(weight<.2)weight*=weight*weight/.04;
        result+=ddgi_read_irradiance(index,normal).rgb*weight;total+=weight;validWeight+=tri;
    }
    coverage=fade*saturate(validWeight);
    return total>1e-10?max(result/total,0):float3(0);
}
float3 ddgi_debug(float3 position,float3 normal,uint mode) {
    if(ddgiGrid.w==0)return float3(.15,0,.15);
    int3 cell=int3(round(position/ddgiParameters.x));
    if(any(cell<ddgiOrigin.xyz)||any(cell>=ddgiOrigin.xyz+int3(ddgiGrid.xyz)))return float3(0);
    uint index=ddgi_index(cell);DDGIProbe probe=ddgiProbes[index];
    bool valid=all(ddgiControls[index].cell==cell)&&probe.metadata.x==ddgiControls[index].version;
    if(!valid)return float3(1,0,1);
    if(mode==1) {
        float3 irradiance=ddgi_read_irradiance(index,normal).rgb;return irradiance/(1+irradiance);
    }
    float3 delta=position-(float3(cell)*ddgiParameters.x+probe.offset.xyz);
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
