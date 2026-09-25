#include "../../../octaryn-client/Shaders/BlockTransportGI/Types.slang"
StructuredBuffer<BtSurface> btSurfaces;
uniform uint4 btFrameInfo;
#include "../../../octaryn-client/Shaders/BlockTransportGI/Work.slang"
RWStructuredBuffer<uint> probeAccepted;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=btWork.y)return;
    uint row;probeAccepted[id.x]=bt_work_row(id.x,row)?row:BT_INVALID;
}
