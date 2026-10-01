#define OCTARYN_MAP_ONLY_RAYS
#define OCTARYN_MAP_DEFERRED_MATERIAL
#include "../../../octaryn-client/Shaders/RayTracing/MapTrace.slang"
RWStructuredBuffer<uint4> mapResults;
[shader("compute")]
[numthreads(32,1,1)]
void probeMapRays(uint lane:SV_DispatchThreadID) {
    if(lane>=19)return;
    float x=lane>=17?float(100*(lane-16)):float(lane*4);
    WorldRayHit hit=map_trace_intersection(float3(x,0,0),float3(0,0,1),10);
    if(!hit.hit) {mapResults[lane]=uint4(0xffffffff);return;}
    MapRayMaterial material=map_ray_material(hit.instance,hit.material);
    hit=world_map_ray_surface(hit,float3(0,0,1));
    float3 expectedNormal=lane==17?normalize(float3(-.25,0,-1)):float3(0,0,-1);
    uint normalValid=all(isfinite(hit.normal)) && dot(hit.normal,expectedNormal)>.999 && !hit.backFace?2u:0u;
    MapVertex vertex=map_ray_vertex(hit.instance,hit.primitive*3);
    bool compact=lane==16 || mapRayMeshes[hit.instance&0x7fffffu].vertexStride==12;
    bool attributesValid=compact?all(vertex.color==1) && all(vertex.uv==0) && all(vertex.uv1==0):
        all(vertex.color==float4(.25,.5,.75,1)) && all(vertex.uv==float2(.125,.25)) && all(vertex.uv1==float2(.5,.75));
    mapResults[lane]=uint4(hit.material,asuint(material.roughness),
        uint(world_ray_dynamic(hit))|normalValid|(attributesValid?4u:0u),asuint(hit.distance));
}
