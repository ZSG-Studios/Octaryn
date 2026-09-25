#include "../../../octaryn-client/Shaders/BlockTransportGI/WorldAdmission.slang"
#include "../../../octaryn-client/Shaders/BlockTransportGI/Sampling.slang"
#include "../../../octaryn-client/Shaders/BlockTransportGI/Hit.slang"
struct LeafQuery {uint4 face,info;};
struct LeafResult {int4 key;float4 reflectance,origin,direction,position;uint4 proof;};
StructuredBuffer<LeafQuery> leafQueries;
RWStructuredBuffer<LeafResult> leafResults;
uniform uint leafCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=leafCount)return;
    LeafQuery query=leafQueries[id.x];LeafResult result=(LeafResult)0;
    BtSurface surface=(BtSurface)0;
    bool admitted=bt_world_surface(query.face,0,surface.key,surface.albedo);
    uint random=bt_hash(query.info.z);float3 origin=0,direction=0,position=0;
    bool sampled=admitted && bt_surface_sample(surface,query.info.x,query.info.y,origin,direction);
    bool direct=admitted && bt_surface_position(surface,query.info.z,random,position);
    result.key=surface.key;result.reflectance=surface.albedo;
    result.origin=float4(sampled?origin:float3(0),sampled?1:0);
    result.direction=float4(sampled?direction:float3(0),0);
    result.position=float4(direct?position:float3(0),direct?1:0);
    result.proof.x=admitted?1:0;
    if(sampled) {
        float3 normal=bt_normal(surface.key.w);
        WorldRayHit hit=world_trace_ray(origin+normal*.2,-normal,.4,false,float3(1e30),false,false,true);
        int4 key=0;bool valid=bt_hit_key(hit,key);
        float4 material=0;bool mapped=valid && bt_surface_material(key,hit.voxel,hit.flags,material);
        result.proof.y=valid && all(key==surface.key)?1:0;
        result.proof.z=mapped && all(material==surface.albedo)?1:0;
        result.proof.w=asuint(hit.albedo.a);
    }
    leafResults[id.x]=result;
}
