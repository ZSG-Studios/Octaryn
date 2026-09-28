#include "../Map/WorldMap.slang"
struct ItemInstance {float4 current;float4 previous;};
StructuredBuffer<ItemInstance> itemInstances;
uniform float4 previousItemView[5];
struct ItemSurfaceOutput {
    MapSurfaceOutput surface;
    float4 previousClip:TEXCOORD7;
    float4 currentClip:TEXCOORD8;
};
float4 item_project(float3 relative) {
    float depth=dot(relative,mapUniforms[5].xyz);
    return float4(dot(relative,mapUniforms[3].xyz)*mapUniforms[6].x,
        dot(relative,mapUniforms[4].xyz)*mapUniforms[6].y,
        depth*mapUniforms[6].z-mapUniforms[6].w,depth);
}
[shader("vertex")]
ItemSurfaceOutput item_vertex(uint id:SV_VertexID,uint instance:SV_InstanceID,uint base:SV_StartInstanceLocation) {
    ItemInstance item=itemInstances[instance+base];
    ItemSurfaceOutput o;o.surface=map_vertex(id);
    o.surface.relative+=item.current.xyz;
    o.currentClip=item_project(o.surface.relative);
    o.surface.position=o.currentClip;
    o.surface.position.xy+=float2(mapUniforms[3].w,mapUniforms[4].w)*o.currentClip.w;
    float3 previous=map_vertex_position(mapVertices[id])+item.previous.xyz-previousItemView[0].xyz;
    float z=dot(previous,previousItemView[3].xyz);
    o.previousClip=float4(dot(previous,previousItemView[1].xyz)*previousItemView[4].x,
        dot(previous,previousItemView[2].xyz)*previousItemView[4].y,0,z);
    return o;
}
[shader("fragment")]
GBufferOutput item_fragment(ItemSurfaceOutput input,bool front:SV_IsFrontFace) {
    GBufferOutput output=map_gbuffer(input.surface,front);output.surfaceFlags=0;return output;
}
[shader("fragment")]
float4 item_motion(ItemSurfaceOutput input,bool front:SV_IsFrontFace):SV_Target0 {
    map_fragment(input.surface,front);
    float2 motion=0;
    if(input.previousClip.w>0 && input.currentClip.w>0)
        motion=(input.previousClip.xy/input.previousClip.w-input.currentClip.xy/input.currentClip.w)*float2(.5,-.5);
    return float4(motion,1,0);
}
