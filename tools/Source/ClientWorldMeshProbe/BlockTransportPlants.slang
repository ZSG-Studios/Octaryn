#include "../../../octaryn-client/Shaders/BlockTransportGI/Sampling.slang"
#include "../../../octaryn-client/Shaders/BlockTransportGI/Hit.slang"
struct PlantQuery {
    uint4 face;float4 toward;int4 key;uint4 options;float4 origin,direction;
};
struct PlantResult {
    uint4 packed;int4 key;float4 origin,direction,normal;uint4 flags;
};
StructuredBuffer<PlantQuery> plantQueries;
RWStructuredBuffer<PlantResult> plantResults;
uniform uint4 plantInfo;
[shader("compute")]
[numthreads(64,1,1)]
void key_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=plantInfo.x)return;
    PlantQuery query=plantQueries[id.x];PlantResult result=(PlantResult)0;
    result.packed=world_plant_surface_key(query.face,query.options.x,query.toward.xyz);
    result.key=int4(asint(result.packed.xyz),int(result.packed.w)-1);
    uint voxel=world_plant_pack_normal(query.options.x,uint(result.key.w));
    result.normal=float4(get_normal(voxel),0);
    result.flags=uint4(world_crossed_plant(query.options.y,query.options.x,(query.face.w>>16)&15)?1:0,
        voxel,world_plant_voxel(voxel)?1:0,0);
    plantResults[id.x]=result;
}
[shader("compute")]
[numthreads(64,1,1)]
void sample_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=plantInfo.x)return;
    PlantQuery query=plantQueries[id.x];PlantResult result=(PlantResult)0;
    float3 origin,direction;bool valid=bt_sample(query.key,query.options.z,query.options.w,origin,direction);
    result.key=query.key;result.origin=float4(origin,1);result.direction=float4(direction,0);
    result.normal=float4(bt_normal(query.key.w),0);result.flags.x=valid?1:0;
    plantResults[id.x]=result;
}
[shader("compute")]
[numthreads(64,1,1)]
void ray_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=plantInfo.x)return;
    PlantQuery query=plantQueries[id.x];PlantResult result=(PlantResult)0;
    WorldRayHit hit=world_trace_ray(query.origin.xyz,query.direction.xyz,query.origin.w,
        false,float3(1e30),false,false,true);
    int4 key;bool valid=bt_hit_key(hit,key);
    result.packed=hit.surfaceKey;result.key=key;
    result.origin=float4(hit.position,hit.distance);result.direction=hit.albedo;
    result.normal=float4(hit.normal,0);
    result.flags=uint4(hit.hit?1:0,valid?1:0,hit.backFace?1:0,hit.material);
    plantResults[id.x]=result;
}
