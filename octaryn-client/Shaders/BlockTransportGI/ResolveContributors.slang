#define BT_CACHE_READONLY
#include "Cache.slang"
#include "Work.slang"
StructuredBuffer<BtCandidate> btCandidates;
RWStructuredBuffer<uint4> btLinks;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=btWork.y*btFrameInfo.w)return;
    uint row;if(!bt_work_row(id.x/btFrameInfo.w,row))return;
    uint slot=id.x%btFrameInfo.w;
    uint address=row*btFrameInfo.w+slot;
    if(btLinks[address].w!=3)return;
    uint target=bt_find(btCandidates[id.x].key);
    if(target==BT_INVALID) {btLinks[address]=uint4(BT_INVALID,0,0,0);return;}
    btLinks[address]=uint4(target,btFrameInfo.x,btSurfaces[target].extra.x,1);
}
