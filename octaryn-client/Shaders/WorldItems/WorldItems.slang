#include "../Materials/MaterialTypes.slang"
StructuredBuffer<AtlasBlockMaterial> blockMaterials;
Texture2DArray<float4> atlasAlbedo;
SamplerState atlasNearest;
SamplerState atlasFiltered;
struct View {float4 camera,right,up,forward,projection;};
struct Item {float4 position;uint4 material;};
uniform View currentView,previousView;
uniform float4 lighting,settings;
uniform Item items[256],previousItems[256];
struct VertexOutput {
    float4 position:SV_Position;
    float2 uv:TEXCOORD0;
    float3 normal:TEXCOORD1;
    nointerpolation uint layer:TEXCOORD2;
    float4 currentClip:TEXCOORD3;
    float4 previousClip:TEXCOORD4;
};
float3 item_point(Item item,View view,float3 local,float time) {
    float angle=time*1.6+item.position.w;
    float3 p=float3(cos(angle)*local.x-sin(angle)*local.z,local.y,sin(angle)*local.x+cos(angle)*local.z);
    precise float3 relative=item.position.xyz-view.camera.xyz;
    return relative+p*.25+float3(0,.1+.025*sin(time*2.5+item.position.w),0);
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
    const float3 normals[6]={float3(-1,0,0),float3(1,0,0),float3(0,-1,0),float3(0,1,0),float3(0,0,-1),float3(0,0,1)};
    float3 p;
    if(face==0)p=float3(0,uv.y,uv.x);
    else if(face==1)p=float3(1,uv.y,1-uv.x);
    else if(face==2)p=float3(uv.x,0,uv.y);
    else if(face==3)p=float3(uv.x,1,1-uv.y);
    else if(face==4)p=float3(1-uv.x,uv.y,0);
    else p=float3(uv.x,uv.y,1);
    Item item=items[instance];
    // CPU upload bounds this index against the bound atlas material buffer.
    AtlasBlockMaterial material=blockMaterials[item.material.x];
    bool sprite=(material.flags&2)!=0;
    if(sprite)p=float3(uv.x,uv.y,.5);
    VertexOutput output;
    output.currentClip=project(currentView,item_point(item,currentView,p-.5,settings.y));
    output.previousClip=project(previousView,item_point(previousItems[instance],previousView,p-.5,settings.z));
    if(item.material.z!=0)output.previousClip=output.currentClip;
    output.position=output.currentClip;
    output.position.xy+=float2(currentView.right.w,currentView.up.w)*output.position.w;
    if(sprite&&face!=0)output.position=float4(2,2,2,1); // Degenerate clipped triangles; one two-sided sprite plane.
    output.uv=float2(uv.x,1-uv.y);output.layer=material.layers[sprite?5:face];
    float angle=settings.y*1.6+item.position.w;float3 n=sprite?float3(0,0,1):normals[face];
    output.normal=float3(cos(angle)*n.x-sin(angle)*n.z,n.y,sin(angle)*n.x+cos(angle)*n.z);
    return output;
}
float4 shade(VertexOutput input) {
    uint width,height,layers;atlasAlbedo.GetDimensions(width,height,layers);
    float2 dimensions=float2(width,height),texel=.5/dimensions;
    float2 dx=ddx(input.uv)*exp2(settings.w),dy=ddy(input.uv)*exp2(settings.w);
    float footprint=max(length(dx*dimensions),length(dy*dimensions));
    float4 sharp=atlasAlbedo.SampleLevel(atlasNearest,float3(clamp(input.uv,texel,1-texel),input.layer),0);
    float4 filtered=atlasAlbedo.SampleGrad(atlasFiltered,float3(input.uv,input.layer),dx,dy);
    float4 color=lerp(sharp,filtered,smoothstep(.5,1,footprint));
    if(color.a<.1)discard;
    float sun=saturate(dot(normalize(input.normal),normalize(lighting.xyz)));
    return float4(color.rgb*(settings.x+lighting.w*sun),1);
}
[shader("fragment")]
float4 fragment_main(VertexOutput input):SV_Target0{return shade(input);}
struct TemporalOutput {float4 color:SV_Target0;float4 motion:SV_Target1;};
[shader("fragment")]
TemporalOutput temporal_fragment_main(VertexOutput input) {
    TemporalOutput output;output.color=shade(input);
    float2 previous=input.previousClip.xy/max(input.previousClip.w,1e-6);
    float2 current=input.currentClip.xy/max(input.currentClip.w,1e-6);
    output.motion=float4((previous-current)*float2(.5,-.5),1,0);return output;
}
