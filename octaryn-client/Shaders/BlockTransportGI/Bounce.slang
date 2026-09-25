#include "Types.slang"
StructuredBuffer<BtSurface> btSurfaces;
StructuredBuffer<uint4> btLinks;
StructuredBuffer<float4> btDirect,btEnvironment,btB0,btB1,btB2;
RWStructuredBuffer<float4> btOutput;
uniform uint4 btSolve; // epoch, capacity, order, links per patch
uniform uint4 btFrameInfo; // geometry epoch, frame, capacity, links per patch
float3 bt_order_value(uint target,uint order) {
    if(btSurfaces[target].state.z==BT_INVALID || btSurfaces[target].state.w==0)return 0;
    float4 value=order==0?btB0[target]:order==1?btB1[target]:btB2[target];
    return bt_epoch(value.w,btSolve.x)?value.rgb:float3(0);
}
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=btSolve.y)return;
    BtSurface surface=btSurfaces[id.x];
    if(surface.state.x!=btFrameInfo.x || surface.state.z==BT_INVALID || surface.state.w==0) {
        btOutput[id.x]=0;return;
    }
    if(btSolve.z==0) {
        bool directValid=bt_epoch(btDirect[id.x].w,btSolve.x);
        bool environmentValid=bt_epoch(btEnvironment[id.x].w,btSolve.x);
        float3 direct=directValid?btDirect[id.x].rgb:float3(0);
        float3 environment=environmentValid?btEnvironment[id.x].rgb:float3(0);
        btOutput[id.x]=directValid || environmentValid?
            float4(surface.albedo.rgb*(direct+environment),asfloat(btSolve.x)):float4(0);
        return;
    }
    float4 history=0;
    bool historyValid=false;
    if(btSolve.z==3) {
        history=btOutput[id.x];
        historyValid=bt_epoch(history.w,btSolve.x) && all(isfinite(history.rgb));
        if(surface.state.z!=btFrameInfo.y) {
            btOutput[id.x]=historyValid?history:float4(0);return;
        }
    }
    float3 gathered=0;
    for(uint slot=0;slot<btSolve.w;++slot) {
        uint4 link=btLinks[id.x*btSolve.w+slot];
        if(link.w!=1 || link.x>=btSolve.y || link.y!=btFrameInfo.x)continue;
        if(btSurfaces[link.x].state.x!=link.y || btSurfaces[link.x].extra.x!=link.z)continue;
        if(btSolve.z==3)gathered+=bt_order_value(link.x,0)+bt_order_value(link.x,1)+bt_order_value(link.x,2);
        else gathered+=bt_order_value(link.x,btSolve.z-1);
    }
    // Unknown and unsupported links retain their share of the denominator.
    gathered/=float(btSolve.w);
    if(btSolve.z!=3)gathered*=surface.albedo.rgb;
    gathered=max(gathered,0);
    if(btSolve.z==3 && historyValid && surface.state.w>1)
        gathered=lerp(history.rgb,gathered,1.0/float(min(surface.state.w,16u)));
    btOutput[id.x]=all(isfinite(gathered))?float4(gathered,asfloat(btSolve.x)):float4(0);
}
