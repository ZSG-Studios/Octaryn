#include "../../../octaryn-client/Shaders/WorldItems/Lighting.slang"
struct ItemLightingInput {
    float4 color,settings,normal,view,light,incidentFront,incidentBack,directFront,directBack;
};
StructuredBuffer<ItemLightingInput> itemLightingInputs;
RWStructuredBuffer<float4> itemLightingOutputs;
uniform uint itemLightingCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=itemLightingCount)return;
    ItemLightingInput v=itemLightingInputs[id.x];
    itemLightingOutputs[id.x]=float4(item_surface_lighting(v.color.rgb,v.color.w,v.settings.x,v.settings.z!=0,
        v.normal.xyz,v.view.xyz,v.light.xyz,v.settings.y,v.incidentFront.rgb,v.incidentBack.rgb,
        v.directFront,v.directBack),1);
}
