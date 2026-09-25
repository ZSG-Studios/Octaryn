#include "../Materials/MaterialTypes.slang"
StructuredBuffer<AtlasBlockMaterial> blockMaterials;
Texture2DArray<float4> atlasAlbedo;
Texture2DArray<float4> atlasSpecular;
SamplerState atlasNearest;
SamplerState atlasFiltered;
struct View {float4 camera,right,up,forward,projection;};
struct Item {float4 position;uint4 material;};
uniform View currentView,previousView;
uniform float4 lighting,settings;
uniform Item items[256],previousItems[256];
#include "../BlockTransportGI/DynamicSample.slang"
#include "Geometry.slang"
#include "Lighting.slang"
struct VertexOutput {
    float4 position:SV_Position;
    float2 uv:TEXCOORD0;
    float3 normal:TEXCOORD1;
    nointerpolation uint layer:TEXCOORD2;
    float4 currentClip:TEXCOORD3;
    float4 previousClip:TEXCOORD4;
    float3 indirectFront:TEXCOORD5;
    float3 indirectBack:TEXCOORD6;
    float4 directFront:TEXCOORD7;
    float4 directBack:TEXCOORD8;
    float3 relative:TEXCOORD9;
};
float3 item_point(Item item,View view,float3 local,float time) {
    precise float3 relative=item.position.xyz-view.camera.xyz;
    return relative+item_offset(item.position.w,local,time);
}
float4 project(View v,float3 relative) {
    float depth=dot(relative,v.forward.xyz);
    return float4(dot(relative,v.right.xyz)*v.projection.x,dot(relative,v.up.xyz)*v.projection.y,
        depth*v.projection.z-v.projection.w,depth);
}
[shader("vertex")]
VertexOutput vertex_main(uint vertex:SV_VertexID,uint instance:SV_InstanceID) {
    const float2 corners[6]={float2(0,0),float2(1,0),float2(1,1),float2(0,0),float2(1,1),float2(0,1)};
    uint face=vertex/6;float2 uv=corners[vertex%6];
    Item item=items[instance];
    // CPU upload bounds this index against the bound atlas material buffer.
    AtlasBlockMaterial material=blockMaterials[item.material.x];
    bool sprite=(material.flags&2)!=0;
    float3 local=item_face_point(face,uv,sprite);
    VertexOutput output;
    output.indirectFront=bt_dynamic_incident(item.material.w+face*2);
    output.indirectBack=bt_dynamic_incident(item.material.w+face*2+1);
    output.directFront=bt_dynamic_direct(item.material.w+face*2);
    output.directBack=bt_dynamic_direct(item.material.w+face*2+1);
    float3 relative=item_point(item,currentView,local,settings.y);output.relative=relative;
    output.currentClip=project(currentView,relative);
    output.previousClip=project(previousView,item_point(previousItems[instance],previousView,local,settings.z));
    if(item.material.z!=0)output.previousClip=output.currentClip;
    output.position=output.currentClip;
    output.position.xy+=float2(currentView.right.w,currentView.up.w)*output.position.w;
    if(sprite&&face!=0)output.position=float4(2,2,2,1); // Degenerate clipped triangles; one two-sided sprite plane.
    output.uv=float2(uv.x,1-uv.y);output.layer=material.layers[sprite?5:face];
    output.normal=item_rotate(item_face_normal(face,sprite),settings.y*1.6+item.position.w);
    return output;
}
float4 shade(VertexOutput input,bool front) {
    uint width,height,layers;atlasAlbedo.GetDimensions(width,height,layers);
    float2 dimensions=float2(width,height),texel=.5/dimensions;
    float2 dx=ddx(input.uv)*exp2(settings.w),dy=ddy(input.uv)*exp2(settings.w);
    float footprint=max(length(dx*dimensions),length(dy*dimensions));
    float4 sharp=atlasAlbedo.SampleLevel(atlasNearest,float3(clamp(input.uv,texel,1-texel),input.layer),0);
    float4 filtered=atlasAlbedo.SampleGrad(atlasFiltered,float3(input.uv,input.layer),dx,dy);
    float4 color=lerp(sharp,filtered,smoothstep(.5,1,footprint));
    if(color.a<.1)discard;
    float4 specular=atlasSpecular.SampleLevel(atlasNearest,float3(clamp(input.uv,texel,1-texel),input.layer),0);
    float metal=specular.g>=230.0/255.0?1:0;
    float roughness=clamp(pow(1-saturate(specular.r),2),.02,1);
    float3 view=-input.relative/max(length(input.relative),1e-6);
    float3 shaded=item_surface_lighting(color.rgb,metal,roughness,front,input.normal,view,normalize(lighting.xyz),
        lighting.w,input.indirectFront,input.indirectBack,input.directFront,input.directBack);
    return float4(max(shaded,0),1);
}
[shader("fragment")]
float4 fragment_main(VertexOutput input,bool front:SV_IsFrontFace):SV_Target0{return shade(input,front);}
struct TemporalOutput {float4 color:SV_Target0;float4 motion:SV_Target1;};
[shader("fragment")]
TemporalOutput temporal_fragment_main(VertexOutput input,bool front:SV_IsFrontFace) {
    TemporalOutput output;output.color=shade(input,front);
    float2 previous=input.previousClip.xy/max(input.previousClip.w,1e-6);
    float2 current=input.currentClip.xy/max(input.currentClip.w,1e-6);
    output.motion=float4((previous-current)*float2(.5,-.5),1,0);return output;
}
