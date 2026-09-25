#include "../../../octaryn-client/Shaders/BlockTransportGI/LocalAreaPosition.slang"
struct BtSampleQuery {int4 key;uint4 info;};
struct BtSampleResult {float4 origin,direction,direct,sequence,directSequence;};
StructuredBuffer<BtSampleQuery> probeSampleQueries;
RWStructuredBuffer<BtSampleResult> probeSampleResults;
uniform uint probeSampleCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=probeSampleCount)return;
    BtSampleQuery query=probeSampleQueries[id.x];
    float3 origin,direction,position;uint random=bt_hash(bt_key_hash(query.key)^bt_hash(query.info.z));
    bool transport=bt_sample(query.key,query.info.y,query.info.x,origin,direction);
    BtSurface surface=(BtSurface)0;surface.key=query.key;
    bool direct=query.info.w==0?bt_direct_position(query.key,query.info.z,random,position):
        bt_local_area_position(surface,query.info.z,query.info.w-1,random,position);
    BtSampleResult result;
    result.origin=float4(origin,transport?1:0);result.direction=float4(direction,0);
    result.direct=float4(position,direct?1:0);
    result.sequence=bt_transport_sequence(query.key,query.info.y,query.info.x);
    result.directSequence=query.info.w==0?bt_sequence(query.info.z,bt_key_hash(query.key)^0x68bc21ebu):float4(0);
    probeSampleResults[id.x]=result;
}
