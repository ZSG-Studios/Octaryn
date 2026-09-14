// Original transparent.frag.glsl: fluid tint/absorption, reflection, ripples and lava glow.
#include "../Sky/Fog.slang"
float2 fluidFaceUv(float3 world,uint voxel,float2 authored,float strength) {
  uint direction=get_direction(voxel);
  if(direction==kDirectionUp && strength>0) return authored;
  if(direction==kDirectionNorth) return float2(world.x,-world.y);
  if(direction==kDirectionSouth) return float2(-world.x,-world.y);
  if(direction==kDirectionEast) return float2(-world.z,-world.y);
  if(direction==kDirectionWest) return float2(world.z,-world.y);
  return direction==kDirectionUp?world.xz*float2(1,-1):world.xz;
}
float4 fluidSampleAtlas(Texture2DArray<float4> atlas,float2 uv,uint voxel) {
  // Original forward pass uses the linear/anisotropic atlas sampler, unlike cutouts.
  float gradientScale=exp2(drawSettings.z);
  return atlas.SampleGrad(atlasLinear,float3(uv,float(get_index(voxel))),ddx(uv)*gradientScale,ddy(uv)*gradientScale);
}
float4 shadeOriginalFluid(VertexOutput input,bool front) {
  uint voxel=input.voxel,direction=get_direction(voxel);
  bool lava=(input.flags&512)!=0,side=direction<4,moving=direction==4 && input.fluid.z>0;
  uint still=lava?27:16,flow=lava?28:26;
  uint chosen=(side||moving)?flow:still;
  uint sampleVoxel=(voxel&~248u)|(chosen<<3);
  float2 uv=fluidFaceUv(input.relative+cameraPosition.xyz,voxel,input.uv,input.fluid.z);
  float4 albedo=fluidSampleAtlas(atlasAlbedo,uv,sampleVoxel);
  if(moving) albedo=lerp(fluidSampleAtlas(atlasAlbedo,uv,(voxel&~248u)|(still<<3)),albedo,
      smoothstep(.10,.70,input.fluid.z/15));
  albedo.rgb*=side?.92:1;
  float3 geometric=get_normal(voxel)*(front?1:-1);
  float distance=length(input.relative),viewDepth=abs(dot(input.relative,cameraForward.xyz));
  float3 view=-input.relative/max(distance,.001),normal=geometric;
  if(materialSettings.x!=0) {
    float4 ns=fluidSampleAtlas(atlasNormal,uv,sampleVoxel);
    float3 ripple=normalize(float3((ns.rg*2-1)*.06,1));
    normal=abs(geometric.y)>.5?normalize(float3(ripple.x,geometric.y,ripple.y)):
        normalize(geometric+float3(ripple.x,ripple.y,0)*.04);
  }
  if(dot(normal,view)<0) normal=-normal;
  float up=step(.5,geometric.y),distanceFog=get_fog(viewDepth,220);
  float level=saturate(input.fluid.w/7),levelFill=1-level*.55;
  float fill=lerp(max(saturate(input.fill),.45),levelFill,.15);
  if(lava) {
    float glow=.40+.18*sin(worldTime.x*1.2+dot(uv,float2(4,-2)));
    float3 color=lerp(albedo.rgb,float3(1,.34,.04),.28)+float3(1,.22,.02)*glow;
    return float4(saturate(lerp(color,get_fog_color(float3(1,.18,0),lightDirectionSky.w,worldTime.y),distanceFog*.05)),1);
  }
  // Water animation frames are grayscale; preserve their detail under blue tint.
  albedo.rgb*=float3(.025,.19,.90);
  float fresnel=pow(1-saturate(dot(normal,view)),5);
  float3 halfVector=normalize(-lightDirectionSky.xyz)+view;
  float3 halfDirection=halfVector/max(length(halfVector),.001);
  float specular=pow(saturate(dot(normal,halfDirection)),96)*saturate(lightDirectionSky.w*worldTime.z)*up*worldTime.w;
  float smoothness=.68;
  if(materialSettings.x!=0) smoothness=max(fluidSampleAtlas(atlasSpecular,uv,sampleVoxel).r,smoothness);
  specular*=lerp(.05,.20,smoothness);
  float3 absorption=lerp(float3(.004,.025,.15),float3(.015,.09,.38),fill);
  float3 lit=lerp(float3(.002,.006,.020),absorption,saturate(lightDirectionSky.w*.82+.18));
  float3 sideAbsorption=lerp(lit,float3(.010,.055,.25),.35);
  float3 color=lerp(albedo.rgb,side?sideAbsorption:lit,.42);
  float3 reflected=reflect(-view,normal);reflected.y=max(reflected.y,.03);
  color=lerp(color,get_sky_color_lit(reflected,lightDirectionSky.w,worldTime.y),(.015+fresnel*.07)*up);
  color+=float3(1,.92,.76)*specular;
  color=lerp(color,get_fog_color(float3(0,.14,1),lightDirectionSky.w,worldTime.y),distanceFog*.03);
  float alpha=lerp(.74,.88,fill);if(side) alpha*=.86;alpha=lerp(alpha,.96,fresnel*up);
  return float4(saturate(color),saturate(alpha));
}
