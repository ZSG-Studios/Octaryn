// Original shaders/composite.comp.glsl: sky inverse tone map, ambient and emission.
#include "../Materials/PbrEvaluate.slang"
#include "../Sky/Fog.slang"
#include "../DDGI/DDGISample.slang"
#include "../Lighting/Surface.slang"
#ifdef OCTARYN_COMPOSITE_RAY_SKY
#include "../DDGI/DDGISkyVisibility.slang"
#endif
#ifdef OCTARYN_COMPOSITE_SRC
Texture2D<float4> srcIrradiance;
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
  float3 geometric=lighting_normal(voxel);
  float3 normal=geometric;
  if((voxel&256)!=0 && dot(normal,-position.xyz)<0)normal=-normal;
  float coverage;
  float3 irradiance;
  float3 fallback;
#ifdef OCTARYN_COMPOSITE_SRC
  // Resolve stores true irradiance in RGB and donor coverage in alpha.
  float4 srcSample=srcIrradiance[id.xy];
  coverage=srcSample.a>0?1:0;
  irradiance=max(srcSample.rgb,0);
  fallback=0;
#else
  irradiance=ddgi_sample(position.xyz+eye.xyz,(voxel&256)!=0?geometric:normal,coverage);
  fallback=original_ambient_light(color.a,lighting.x)*lighting.y;
#ifdef OCTARYN_COMPOSITE_RAY_SKY
  if(coverage<.999 && (ddgiGrid.w!=0 || ddgiFineGrid.w!=0))
    fallback=ddgi_visible_sky(position.xyz+eye.xyz,normal,sun.xyz,float4(lighting.xyz,0));
#endif
#endif
  float3 indirect=lerp(fallback,irradiance/3.14159265359,coverage);
  float3 hdr=max(color.rgb*((indirect+direct)*(1-materials[id.xy].g)+emission)+localLighting[id.xy].rgb,0);
  uint debug=uint(dimensions.w);
  if(debug==1) {scene[id.xy]=float4(visibility.xxx,1);return;}
  if(debug>=2 && debug<=7) {scene[id.xy]=float4(ddgi_debug(position.xyz+eye.xyz,normal,debug-1),1);return;}
  if(debug==8) {scene[id.xy]=float4(localLighting[id.xy].rgb,1);return;}
  if(debug==12) {scene[id.xy]=float4(sunHistory[id.xy].y/32,visibility,0,1);return;}
  // Component split: which term lights this pixel? 28 = direct sun (leaks show
  // here), 29 = DDGI bounce (stale probes show here), 30 = ambient fallback
  // with red where the probe cage has no coverage.
  if(debug==28) {scene[id.xy]=float4(color.rgb*direct*(1-materials[id.xy].g),1);return;}
  if(debug==29) {scene[id.xy]=float4(color.rgb*(irradiance/3.14159265359)*coverage*(1-materials[id.xy].g),1);return;}
  if(debug==30) {
    float3 fall=color.rgb*fallback*(1-coverage)*(1-materials[id.xy].g);
    float uncovered=(1-coverage)*step(.001,dot(fallback,float3(.2126,.7152,.0722)));
    scene[id.xy]=float4(fall+float3(.6,0,0)*uncovered,1);return;
  }
  if(debug>=13 && debug<=20) {scene[id.xy]=float4(localLighting[id.xy].rgb,1);return;}
  scene[id.xy]=float4(apply_scene_fog(hdr,position.xyz,lighting.x,lighting.z,lighting.w>0,lighting.w),1);
}
