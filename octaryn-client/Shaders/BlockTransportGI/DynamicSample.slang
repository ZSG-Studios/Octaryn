#ifndef OCTARYN_BLOCK_TRANSPORT_DYNAMIC_SAMPLE
#define OCTARYN_BLOCK_TRANSPORT_DYNAMIC_SAMPLE
StructuredBuffer<float4> btDynamicValues;
StructuredBuffer<float4> btDynamicDirect;
uniform uint4 btDynamicInfo;
float3 bt_dynamic_incident(uint row) {
    return btDynamicInfo.x!=0 && row<btDynamicInfo.y?btDynamicValues[row].rgb:float3(0);
}
float4 bt_dynamic_direct(uint row) {
    return btDynamicInfo.x!=0 && row<btDynamicInfo.y?btDynamicDirect[row]:float4(0,0,0,btDynamicInfo.z!=0?0:1);
}
#endif
