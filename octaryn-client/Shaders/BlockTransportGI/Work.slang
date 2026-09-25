#ifndef OCTARYN_BLOCK_TRANSPORT_WORK
#define OCTARYN_BLOCK_TRANSPORT_WORK
StructuredBuffer<uint4> btWorkRows;
#ifdef BT_WORK_WRITABLE_SCHEDULE
RWStructuredBuffer<uint> btSchedule;
#else
StructuredBuffer<uint> btSchedule;
#endif
uniform uint4 btWork; // reserved, maximum selected rows, radiance epoch, reserved
bool bt_work_row(uint index,out uint row) {
    row=BT_INVALID;
    if(index>=min(btWork.y,btSchedule[2]) || btSchedule[5]!=btFrameInfo.x ||
        btSchedule[6]!=btWork.z || btSchedule[7]!=btFrameInfo.y)return false;
    uint4 selected=btWorkRows[index];
    if(selected.x>=btFrameInfo.z || selected.z!=btFrameInfo.x)return false;
    BtSurface surface=btSurfaces[selected.x];
    if(surface.state.x!=selected.z || surface.extra.x!=selected.y)return false;
    row=selected.x;return true;
}
#endif
