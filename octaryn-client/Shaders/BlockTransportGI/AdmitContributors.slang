#include "Cache.slang"
#include "Work.slang"
StructuredBuffer<BtCandidate> btCandidates;
uniform int4 btContributorMin,btContributorMax;
uniform uint btContributorLimit;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=btWork.y*btFrameInfo.w)return;
    uint row;if(!bt_work_row(id.x/btFrameInfo.w,row))return;
    BtCandidate candidate=btCandidates[id.x];
    if(bt_valid_key(candidate.key) && all(candidate.key.xyz>=btContributorMin.xyz) &&
        all(candidate.key.xyz<btContributorMax.xyz))
        bt_admit(candidate.key,candidate.albedo,false,false,btContributorLimit);
}
