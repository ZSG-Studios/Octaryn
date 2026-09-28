#ifndef OCTARYN_VIRTUAL_GEOMETRY_SLANG
#define OCTARYN_VIRTUAL_GEOMETRY_SLANG
#include "../Map/MapGeometry.slang"
struct GeometryCluster {
    uint group,refinedGroup,material,page;
    uint vertexOffset,vertexCount,triangleOffset,triangleCount,flags;
    float3 center;float radius,error;
};
struct GeometryPage {uint slot,generation,resident,reserved;};
StructuredBuffer<GeometryCluster> geometryClusters;
StructuredBuffer<uint4> geometrySelected;
StructuredBuffer<uint> geometryCounters;
StructuredBuffer<GeometryPage> geometryPageTable;
ByteAddressBuffer geometryPool;
StructuredBuffer<MapRayMaterial> geometryMaterials;
uniform float4 geometryView[5]; // eye, right+jitterX, up+jitterY, forward, projection
uniform uint4 geometryExtent; // width, height, selected count, pool slots
uint geometry_selected_count() {return min(geometryCounters[0],geometryExtent.z);}

bool geometry_valid(uint4 selected) {
    GeometryPage page=geometryPageTable[selected.y];
    return page.resident!=0 && page.slot==selected.z && page.generation==selected.w && page.slot<geometryExtent.w;
}
MapVertex geometry_vertex(GeometryCluster cluster,uint slot,uint index) {
    return geometryPool.Load<MapVertex>(slot*65536+cluster.vertexOffset+index*80);
}
uint3 geometry_triangle(GeometryCluster cluster,uint slot,uint triangle) {
    uint packed=geometryPool.Load(slot*65536+cluster.triangleOffset+triangle*4);
    return uint3(packed&255,(packed>>8)&255,(packed>>16)&255);
}
float4 geometry_project(MapVertex vertex) {
    float3 relative=map_vertex_position(vertex)-geometryView[0].xyz;
    float z=dot(relative,geometryView[3].xyz);
    return float4(dot(relative,geometryView[1].xyz)*geometryView[4].x+geometryView[1].w*z,
        dot(relative,geometryView[2].xyz)*geometryView[4].y+geometryView[2].w*z,
        z*geometryView[4].z-geometryView[4].w,z);
}
float2 geometry_screen(float4 p) {return (p.xy/p.w*float2(.5,-.5)+.5)*float2(geometryExtent.xy);}
float geometry_edge(float2 a,float2 b,float2 p) {return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);}
bool geometry_software(GeometryCluster cluster,float4 a,float4 b,float4 c) {
    if((cluster.flags&3)!=0 || min(a.w,min(b.w,c.w))<=0 || min(a.z,min(b.z,c.z))<=0 ||
        max(a.z/a.w,max(b.z/b.w,c.z/c.w))>=1)return false;
    float2 p=geometry_screen(a),q=geometry_screen(b),r=geometry_screen(c);
    float2 extent=ceil(max(p,max(q,r)))-floor(min(p,min(q,r)));
    return all(extent<=4) && all(extent>0);
}
// Perspective-correct barycentrics and derivatives evaluated at a pixel center.
bool geometry_interpolation(float4 a,float4 b,float4 c,float2 pixel,
    out float3 weights,out float3 dx,out float3 dy) {
    float2 p=geometry_screen(a),q=geometry_screen(b),r=geometry_screen(c);
    float area=geometry_edge(p,q,r);
    weights=dx=dy=0;
    if(abs(area)<1e-10)return false;
    float3 lambda=float3(geometry_edge(q,r,pixel),geometry_edge(r,p,pixel),geometry_edge(p,q,pixel))/area;
    float3 lx=float3(q.y-r.y,r.y-p.y,p.y-q.y)/area;
    float3 ly=float3(r.x-q.x,p.x-r.x,q.x-p.x)/area;
    float3 reciprocal=1/float3(a.w,b.w,c.w);
    float3 numerator=lambda*reciprocal;
    float denominator=dot(numerator,1);
    if(abs(denominator)<1e-20)return false;
    weights=numerator/denominator;
    dx=(lx*reciprocal-weights*dot(lx,reciprocal))/denominator;
    dy=(ly*reciprocal-weights*dot(ly,reciprocal))/denominator;
    return true;
}
#endif
