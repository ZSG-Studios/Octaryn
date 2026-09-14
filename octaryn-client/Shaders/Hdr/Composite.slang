// Original shaders/composite.comp.glsl: sky inverse tone map, ambient and emission.
#include "../Materials/PbrEvaluate.slang"
#include "../Sky/Fog.slang"
#include "../DDGI/DDGISample.slang"
#include "../Lighting/Surface.slang"
#ifdef OCTARYN_DDGI_SKY_TRACE
#include "../DDGI/DDGISkyVisibility.slang"
#endif
Texture2D<float4> colors;
Texture2D<float4> positions;
Texture2D<float4> voxels;
Texture2D<float4> materials;
RWTexture2D<float4> scene;
Texture2D<float> sunVisibility;
Texture2D<float4> localLighting;
Texture2D<float2> sunHistory;
uniform float4 eye;
uniform float4 lighting;
uniform float4 sun; // Toward-sun direction and time-scaled user strength.
uniform float4 dimensions; // Active render extent; allocation may be larger.
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
  if(any(id.xy>=uint2(dimensions.xy)))return;
  float4 color=colors[id.xy],position=positions[id.xy];
  if(all(voxels[id.xy]==0) && position.w==0) {
    float3 ldr=clamp(color.rgb,0,.999);scene[id.xy]=float4(ldr/(1-ldr),1);return;
  }
  float emission=clamp(materials[id.xy].a,0,8);
  uint4 packed=uint4(round(voxels[id.xy]*255));
  uint voxel=packed.x|(packed.y<<8)|(packed.z<<16)|(packed.w<<24);
  const float3 normals[6]={float3(0,0,1),float3(0,0,-1),float3(1,0,0),float3(-1,0,0),float3(0,1,0),float3(0,-1,0)};
  float nl=dot(normals[min(voxel&7,5u)],sun.xyz);
  if((voxel&256)!=0) nl=abs(nl);
  float visibility=dimensions.z!=0?sunVisibility[id.xy]:1;
  float direct=max(nl,0)*sun.w*visibility/3.14159265359;
  float3 normal=lighting_normal(voxel);
  if((voxel&256)!=0 && dot(normal,-position.xyz)<0)normal=-normal;
  float coverage;
  float3 irradiance=ddgi_sample(position.xyz+eye.xyz,normal,coverage);
  float3 fallback=original_ambient_light(color.a,lighting.x)*lighting.y;
#ifdef OCTARYN_DDGI_SKY_TRACE
  if(ddgiGrid.w!=0 && coverage<1)
    fallback=ddgi_visible_sky(position.xyz+eye.xyz,normal,sun.xyz,
        float4(lighting.xy,lighting.z,0));
#endif
  float3 indirect=lerp(fallback,irradiance/3.14159265359,coverage);
  float3 hdr=max(color.rgb*((indirect+direct)*(1-materials[id.xy].g)+emission)+localLighting[id.xy].rgb,0);
  uint debug=uint(dimensions.w);
  if(debug==1) {scene[id.xy]=float4(visibility.xxx,1);return;}
  if(debug>=2 && debug<=7) {scene[id.xy]=float4(ddgi_debug(position.xyz+eye.xyz,normal,debug-1),1);return;}
  if(debug==8) {scene[id.xy]=float4(localLighting[id.xy].rgb,1);return;}
  if(debug==12) {scene[id.xy]=float4(sunHistory[id.xy].y/32,visibility,0,1);return;}
  if(debug>=13 && debug<=20) {scene[id.xy]=float4(localLighting[id.xy].rgb,1);return;}
  scene[id.xy]=float4(apply_scene_fog(hdr,position.xyz,lighting.x,lighting.z,lighting.w>0,lighting.w),1);
}
