#define BT_CACHE_READONLY
#include "../../../octaryn-client/Shaders/BlockTransportGI/Cache.slang"
#include "../../../octaryn-client/Shaders/BlockTransportGI/Hit.slang"
void bt_record_player_history_query() {}
#include "../../../octaryn-client/Shaders/BlockTransportGI/ActorHistory.slang"
struct ActorHistoryQuery {uint4 previous;float4 origin,direction;};
StructuredBuffer<ActorHistoryQuery> actorHistoryQueries;
RWStructuredBuffer<float4> actorHistoryResults;
uniform uint actorHistoryCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=actorHistoryCount)return;
    ActorHistoryQuery q=actorHistoryQueries[id.x];float limit;
    bool valid=bt_previous_world_limit(q.previous,q.origin.xyz,q.direction.xyz,limit);
    actorHistoryResults[id.x]=float4(limit,valid?1:0,0,0);
}
