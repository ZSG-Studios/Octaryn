#include "Visibility.slang"
struct RasterVertex {
    float4 position:SV_Position;
    float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;float4 color:TEXCOORD2;
    nointerpolation uint selected:TEXCOORD3;
};
struct Payload {uint selected[32];};
struct RasterPrimitive {uint triangle:SV_PrimitiveID;};
groupshared Payload rasterPayload;
groupshared uint rasterVertexCount,rasterTriangleCount,rasterGroupCount;
StructuredBuffer<uint> geometryOcclusionFlags;
// Bin lists are fully GPU-driven: element 0 holds the appended cluster count,
// selected slot ids follow from element 1. No per-pass CPU uniforms.
StructuredBuffer<uint> geometrySoftwareBin;
StructuredBuffer<uint> geometryHardwareBin;
uniform uint geometryOcclusionPhase;
bool raster_visible(uint selected) {
    return geometryOcclusionPhase==0 || geometryOcclusionFlags[selected]==geometryOcclusionPhase;
}
RasterVertex raster_vertex(GeometryCluster cluster,uint4 selected,uint lane) {
    MapVertex vertex=geometry_vertex(cluster,selected.z,lane,selected.x);RasterVertex output;
    output.position=geometry_project(vertex);output.uv=vertex.uv;output.uv1=vertex.uv1;
    output.color=vertex.color;output.selected=lane;return output;
}
[shader("amplification")]
[numthreads(32,1,1)]
void amplification_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    uint dispatchGroup=group.x+group.y*65535;
    uint selected=dispatchGroup*32+lane;
    if(lane==0)rasterGroupCount=0;
    GroupMemoryBarrierWithGroupSync();
    uint total=geometry_selected_count();
    if(selected<total && raster_visible(selected)) {
        uint slot;InterlockedAdd(rasterGroupCount,1,slot);rasterPayload.selected[slot]=selected;
    }
    GroupMemoryBarrierWithGroupSync();
    DispatchMesh(rasterGroupCount,1,1,rasterPayload);
}
[shader("amplification")]
[numthreads(32,1,1)]
void amplification_binned_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    uint dispatchGroup=group.x+group.y*65535;
    uint entry=dispatchGroup*32+lane;
    if(lane==0)rasterGroupCount=0;
    GroupMemoryBarrierWithGroupSync();
    if(entry<geometryHardwareBin[0]) {
        uint slot;InterlockedAdd(rasterGroupCount,1,slot);rasterPayload.selected[slot]=geometryHardwareBin[entry+1];
    }
    GroupMemoryBarrierWithGroupSync();
    DispatchMesh(rasterGroupCount,1,1,rasterPayload);
}
[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128,1,1)]
void mesh_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex,in payload Payload payload,
    out vertices RasterVertex vertices[128],out indices uint3 triangles[128],out primitives RasterPrimitive primitiveData[128]) {
    uint index=payload.selected[group.x];uint4 selected=geometrySelected[index];
    GeometryCluster cluster=geometry_cluster(selected.x);
    bool valid=geometry_valid(selected);
    if(lane==0) {rasterVertexCount=valid?cluster.vertexCount:0;rasterTriangleCount=valid?cluster.triangleCount:0;}
    GroupMemoryBarrierWithGroupSync();
    SetMeshOutputCounts(rasterVertexCount,rasterTriangleCount);
    if(valid && lane<cluster.vertexCount) {
        RasterVertex output=raster_vertex(cluster,selected,lane);output.selected=index;vertices[lane]=output;
    }
    if(valid && lane<cluster.triangleCount) {
        primitiveData[lane].triangle=lane;
        uint3 tri=geometry_triangle(cluster,selected.z,lane,selected.x);
        float4 a=geometry_project(geometry_vertex(cluster,selected.z,tri.x,selected.x));
        float4 b=geometry_project(geometry_vertex(cluster,selected.z,tri.y,selected.x));
        float4 c=geometry_project(geometry_vertex(cluster,selected.z,tri.z,selected.x));
        triangles[lane]=geometry_software(cluster,a,b,c)?uint3(0):tri;
    }
}
[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128,1,1)]
void mesh_binned_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex,in payload Payload payload,
    out vertices RasterVertex vertices[128],out indices uint3 triangles[128],out primitives RasterPrimitive primitiveData[128]) {
    uint index=payload.selected[group.x];uint4 selected=geometrySelected[index];
    GeometryCluster cluster=geometry_cluster(selected.x);
    bool valid=geometry_valid(selected);
    if(lane==0) {rasterVertexCount=valid?cluster.vertexCount:0;rasterTriangleCount=valid?cluster.triangleCount:0;}
    GroupMemoryBarrierWithGroupSync();
    SetMeshOutputCounts(rasterVertexCount,rasterTriangleCount);
    if(valid && lane<cluster.vertexCount) {
        RasterVertex output=raster_vertex(cluster,selected,lane);output.selected=index;vertices[lane]=output;
    }
    // Binned clusters routed here are hardware-class; no per-triangle re-test.
    if(valid && lane<cluster.triangleCount) {
        primitiveData[lane].triangle=lane;
        triangles[lane]=geometry_triangle(cluster,selected.z,lane,selected.x);
    }
}
[shader("fragment")]
void fragment_main(RasterVertex input,uint triangle:SV_PrimitiveID,bool front:SV_IsFrontFace) {
    uint4 selected=geometrySelected[input.selected];GeometryCluster cluster=geometry_cluster(selected.x);
    if(!front && (cluster.flags&256)==0)discard;
    MapRayMaterial material=geometryMaterials[cluster.material];
    if(material.alphaMode==2)discard;
    if(material.alphaMode==1) {
        float alpha=material.baseColor.a*input.color.a*map_texture_sample(material.textures[0],input.uv,input.uv1,
            ddx(input.uv),ddy(input.uv),ddx(input.uv1),ddy(input.uv1)).a;
        if(alpha<material.alphaCutoff)discard;
    }
    geometry_commit(uint2(input.position.xy),input.position.z,input.selected,triangle,front);
}
bool top_left(float2 a,float2 b) {float2 d=b-a;return d.y<0 || (d.y==0 && d.x>0);}
bool edge_inside(float edge,bool inclusive) {return edge>0 || (edge==0 && inclusive);}
void software_triangle(GeometryCluster cluster,uint4 selected,uint index,uint lane) {
    uint3 tri=geometry_triangle(cluster,selected.z,lane,selected.x);
    float4 a=geometry_project(geometry_vertex(cluster,selected.z,tri.x,selected.x));
    float4 b=geometry_project(geometry_vertex(cluster,selected.z,tri.y,selected.x));
    float4 c=geometry_project(geometry_vertex(cluster,selected.z,tri.z,selected.x));
    // Raster grid uses eight fractional bits, matching the hardware coverage grid.
    float2 p=round(geometry_screen(a)*256),q=round(geometry_screen(b)*256),r=round(geometry_screen(c)*256);
    float area=geometry_edge(p,q,r);if(area==0)return;
    bool front=area<0;
    // Counter-clockwise front faces have negative area in the y-down viewport.
    if(area>0 && (cluster.flags&256)==0)return;
    if(area<0) {float2 temp=q;q=r;r=temp;float4 clip=b;b=c;c=clip;area=-area;}
    int2 lower=max(int2(ceil(min(p,min(q,r))/256-.5)),int2(0));
    int2 upper=min(int2(floor(max(p,max(q,r))/256-.5)),int2(geometryExtent.xy)-1);
    bool pq=top_left(p,q),qr=top_left(q,r),rp=top_left(r,p);
    for(int y=lower.y;y<=upper.y;++y)for(int x=lower.x;x<=upper.x;++x) {
        float2 sample=float2(x+.5,y+.5)*256;
        float3 e=float3(geometry_edge(q,r,sample),geometry_edge(r,p,sample),geometry_edge(p,q,sample));
        if(edge_inside(e.x,qr) && edge_inside(e.y,rp) && edge_inside(e.z,pq)) {
            float depth=dot(e/area,float3(a.z/a.w,b.z/b.w,c.z/c.w));
            geometry_commit(uint2(x,y),depth,index,lane,front);
        }
    }
}
[shader("compute")]
[numthreads(128,1,1)]
void software_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    uint index=group.x+group.y*65535;if(index>=geometry_selected_count())return;
    if(!raster_visible(index))return;
    uint4 selected=geometrySelected[index];if(!geometry_valid(selected))return;
    GeometryCluster cluster=geometry_cluster(selected.x);if(lane>=cluster.triangleCount)return;
    uint3 tri=geometry_triangle(cluster,selected.z,lane,selected.x);
    float4 a=geometry_project(geometry_vertex(cluster,selected.z,tri.x,selected.x));
    float4 b=geometry_project(geometry_vertex(cluster,selected.z,tri.y,selected.x));
    float4 c=geometry_project(geometry_vertex(cluster,selected.z,tri.z,selected.x));
    if(!geometry_software(cluster,a,b,c))return;
    software_triangle(cluster,selected,index,lane);
}
[shader("compute")]
[numthreads(128,1,1)]
void software_binned_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    uint groupIndex=group.x+group.y*65535;
    if(groupIndex>=geometrySoftwareBin[0])return;
    uint index=geometrySoftwareBin[groupIndex+1];
    uint4 selected=geometrySelected[index];if(!geometry_valid(selected))return;
    GeometryCluster cluster=geometry_cluster(selected.x);if(lane>=cluster.triangleCount)return;
    // Software-binned clusters satisfy the conservative sphere test, so every
    // triangle is rasterized without re-projection.
    software_triangle(cluster,selected,index,lane);
}
