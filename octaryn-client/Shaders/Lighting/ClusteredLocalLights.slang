#include "LocalLight.slang"
RWStructuredBuffer<uint> localTileCounts,localTileLights;
uniform uint4 localTileSettings; // tile count X/Y, list capacity, tile pixel size
uniform uint4 extent;
uniform float4 eye,clusterRight,clusterUp,clusterForward,clusterProjection;
[shader("compute")][numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=extent.w)return;
    LocalLight light=localLights[id.x];
    float radius=light.positionRange.w;
    if(uint(light.axisVType.w)==2)radius+=length(light.axisUInner.xyz)+length(light.axisVType.xyz);
    float3 delta=light.positionRange.xyz-eye.xyz;
    float3 center=float3(dot(delta,clusterRight.xyz),dot(delta,clusterUp.xyz),dot(delta,clusterForward.xyz));
    if(center.z+radius<=.001)return;
    float2 minimum=-1,maximum=1;
    if(center.z-radius>.001) {
        minimum=1e20;maximum=-1e20;
        // The projected bounding cube conservatively encloses the influence sphere.
        for(uint corner=0;corner<8;++corner) {
            float3 point=center+float3((corner&1)!=0?radius:-radius,(corner&2)!=0?radius:-radius,(corner&4)!=0?radius:-radius);
            float2 ndc=point.xy*clusterProjection.xy/point.z;
            minimum=min(minimum,ndc);maximum=max(maximum,ndc);
        }
    }
    if(any(maximum<-1)||any(minimum>1))return;
    float2 lo=float2(minimum.x*.5+.5,.5-maximum.y*.5);
    float2 hi=float2(maximum.x*.5+.5,.5-minimum.y*.5);
    // One-pixel expansion covers raster jitter and edge rounding.
    int2 first=clamp(int2(floor((lo*extent.xy-1)/localTileSettings.w)),int2(0),int2(localTileSettings.xy)-1);
    int2 last=clamp(int2(floor((hi*extent.xy+1)/localTileSettings.w)),int2(0),int2(localTileSettings.xy)-1);
    for(int y=first.y;y<=last.y;++y)for(int x=first.x;x<=last.x;++x) {
        uint tile=y*localTileSettings.x+x,offset;
        InterlockedAdd(localTileCounts[tile],1,offset);
        if(offset<localTileSettings.z)localTileLights[tile*localTileSettings.z+offset]=id.x;
    }
}
