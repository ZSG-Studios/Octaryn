#include "../../../octaryn-client/Shaders/BlockTransportGI/Sampling.slang"
#include "../../../octaryn-client/Shaders/BlockTransportGI/Hit.slang"
struct BoundaryQuery {int4 key;uint4 info;float4 origin,direction;};
struct BoundaryResult {float4 origin,direction,position;uint4 key;float4 hit;};
StructuredBuffer<BoundaryQuery> boundaryQueries;
RWStructuredBuffer<BoundaryResult> boundaryResults;
uniform uint boundaryCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=boundaryCount)return;
    BoundaryQuery q=boundaryQueries[id.x];BoundaryResult result=(BoundaryResult)0;
    float3 origin=0,direction=0,position=0;bool valid=false,direct=false;
    if(q.info.x==1) {
        direct=bt_cube_position(q.key,q.origin.xy,position);
        valid=direct && bt_outward_origin(position,bt_normal(q.key.w),origin);
        direction=bt_normal(q.key.w);
    } else if(q.info.x!=2) {
        valid=bt_sample(q.key,q.info.z,q.info.y,origin,direction);
        uint random=bt_hash(bt_key_hash(q.key)^bt_hash(q.info.w));
        direct=bt_direct_position(q.key,q.info.w,random,position);
    } else {origin=q.origin.xyz;direction=q.direction.xyz;valid=true;}
    result.origin=float4(origin,valid?1:0);result.direction=float4(direction,0);
    result.position=float4(position,direct?1:0);
    if(valid && q.info.x>=2) {
        WorldRayHit hit=world_trace_ray(origin,direction,q.origin.w,false,float3(1e30),false,false,true,q.direction.w);
        int4 key;bool mapped=bt_hit_key(hit,key);
        result.key=hit.surfaceKey;
        result.hit=float4(hit.hit?1:0,hit.distance,hit.backFace?1:0,mapped?1:0);
    }
    boundaryResults[id.x]=result;
}
