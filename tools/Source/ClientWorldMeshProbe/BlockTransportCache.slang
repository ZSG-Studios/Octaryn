#include "../../../octaryn-client/Shaders/BlockTransportGI/Cache.slang"
StructuredBuffer<BtCandidate> probeCandidates;
RWStructuredBuffer<uint4> probeResults;
uniform uint4 probeOptions;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(probeOptions.y==2) {
        if(id.x>=probeOptions.x)return;
        BtCandidate candidate=probeCandidates[id.x];
        probeResults[id.x]=uint4(bt_admit(candidate.key,candidate.albedo,probeOptions.z!=0,false,probeOptions.w),0,0,0);
        return;
    }
    if(id.x!=0)return;
    for(uint i=0;i<probeOptions.x;++i) {
        BtCandidate candidate=probeCandidates[i];
        uint admitted=probeOptions.y==0?bt_admit(candidate.key,candidate.albedo,probeOptions.z!=0,false,probeOptions.w):BT_INVALID;
        probeResults[i]=uint4(admitted,bt_find(candidate.key),bt_key_hash(candidate.key),0);
    }
}
