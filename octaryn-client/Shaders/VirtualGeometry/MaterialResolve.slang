#include "Geometry.slang"
#include "../Lighting/Surface.slang"
#include "../Lighting/DiffuseEnvironment.slang"
StructuredBuffer<uint64_t> geometryVisibilityRead;
uniform float4 geometryAmbient; // ambient profile matches mapUniforms[8].z and [7].w
struct ScreenVertex {float4 position:SV_Position;};
[shader("vertex")]
ScreenVertex vertex_main(uint id:SV_VertexID) {
    ScreenVertex output;float2 p=float2((id<<1)&2,id&2);
    output.position=float4(p*float2(2,-2)+float2(-1,1),0,1);return output;
}
struct ResolveOutput {
    float4 color:SV_Target0;float4 relative:SV_Target1;float4 packedNormal:SV_Target2;
    float4 material:SV_Target3;float4 emission:SV_Target4;
    uint surfaceFlags:SV_Target5;float depth:SV_Depth;
};
[shader("fragment")]
ResolveOutput fragment_main(ScreenVertex input) {
    uint2 pixel=uint2(input.position.xy);
    uint64_t key=geometryVisibilityRead[pixel.y*geometryExtent.x+pixel.x];
    if(key==0)discard;
    uint packedIdentity=~uint(key);
    uint id=(packedIdentity>>1)-1,clusterIndex=id/128,triangle=id%128;
    if(clusterIndex>=geometryExtent.z)discard;
    GeometryCluster cluster=geometry_cluster(clusterIndex);
    GeometryPage page=geometryPageTable[cluster.page];
    uint4 selected=uint4(clusterIndex,cluster.page,page.slot,page.generation);
    if(!geometry_valid(selected))discard;
    if(triangle>=cluster.triangleCount)discard;
    uint3 ids=geometry_triangle(cluster,selected.z,triangle,selected.x);
    MapVertex a=geometry_vertex(cluster,selected.z,ids.x,selected.x);
    MapVertex b=geometry_vertex(cluster,selected.z,ids.y,selected.x);
    MapVertex c=geometry_vertex(cluster,selected.z,ids.z,selected.x);
    float4 ap=geometry_project(a),bp=geometry_project(b),cp=geometry_project(c);
    float3 weights,wx,wy;
    if(!geometry_interpolation(ap,bp,cp,input.position.xy,weights,wx,wy))discard;
    float2 uv=a.uv*weights.x+b.uv*weights.y+c.uv*weights.z;
    float2 uv1=a.uv1*weights.x+b.uv1*weights.y+c.uv1*weights.z;
    float2 ux=a.uv*wx.x+b.uv*wx.y+c.uv*wx.z,uy=a.uv*wy.x+b.uv*wy.y+c.uv*wy.z;
    float2 u1x=a.uv1*wx.x+b.uv1*wx.y+c.uv1*wx.z,u1y=a.uv1*wy.x+b.uv1*wy.y+c.uv1*wy.z;
    float3 pa=map_vertex_position(a),pb=map_vertex_position(b),pc=map_vertex_position(c);
    float3 position=pa*weights.x+pb*weights.y+pc*weights.z;
    float3 normal=normalize((cluster.flags&512)!=0?cross(pb-pa,pc-pa):
        map_vertex_normal(a)*weights.x+map_vertex_normal(b)*weights.y+map_vertex_normal(c)*weights.z);
    float4 tangent=a.tangent*weights.x+b.tangent*weights.y+c.tangent*weights.z;
    float4 color=a.color*weights.x+b.color*weights.y+c.color*weights.z;
    MapRayMaterial m=geometryMaterials[cluster.material];
    if(dot(tangent.xyz,tangent.xyz)<1e-10 || map_texture_frame_changed(m.textures[2])) {
        float2 ua=map_texture_uv(m.textures[2],a.uv,a.uv1);
        float2 ub=map_texture_uv(m.textures[2],b.uv,b.uv1);
        float2 uc=map_texture_uv(m.textures[2],c.uv,c.uv1);
        float2 dx=ua*wx.x+ub*wx.y+uc*wx.z,dy=ua*wy.x+ub*wy.y+uc*wy.z;
        float3 px=pa*wx.x+pb*wx.y+pc*wx.z,py=pa*wy.x+pb*wy.y+pc*wy.z;
        float determinant=dx.x*dy.y-dx.y*dy.x;
        tangent.xyz=abs(determinant)>1e-10?(px*dy.y-py*dx.y)/determinant:float3(0);
        float3 bitangent=abs(determinant)>1e-10?(py*dx.x-px*dy.x)/determinant:float3(0);
        tangent.w=dot(cross(normal,tangent.xyz),bitangent)<0?-1:1;
    }
    MapSurface surface=map_surface(m,uv,uv1,color,normal,tangent.xyz,tangent.w,ux,uy,u1x,u1y);
    // Use the rasterizer's clipped primitive orientation, including near-plane crossings.
    bool front=(packedIdentity&1)==0;
    if(!front)surface.normal=-surface.normal;
    ResolveOutput output;
    output.color=float4(surface.albedo.rgb,original_ambient_visibility(surface.normal,false,geometryAmbient.x,geometryAmbient.y));
    float3 relative=position-geometryView[0].xyz;
    output.relative=float4(relative,dot(relative,geometryView[3].xyz));
    uint packed=lighting_pack_map(surface.normal,surface.occlusion);
    output.packedNormal=float4(packed&255,(packed>>8)&255,(packed>>16)&255,packed>>24)/255.;
    output.material=float4(surface.roughness,surface.metallic,.04,0);output.emission=float4(surface.emission,0);
    output.surfaceFlags=m.alphaMode==0?1u:0u;output.depth=asfloat(~uint(key>>32));return output;
}
