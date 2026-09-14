// Original shaders/composite.comp.glsl: sky inverse tone map, ambient and emission.
#include "../Materials/PbrEvaluate.slang"
#include "../Sky/Fog.slang"
Texture2D<float4> colors;
Texture2D<float4> positions;
Texture2D<float4> voxels;
Texture2D<float4> materials;
RWTexture2D<float4> scene;
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
  // Complete the previously unwired Sun control: unshadowed Lambert diffuse.
  // This does not claim a shadow map, visibility solution, or fallback sunlight.
  float direct=max(nl,0)*sun.w/3.14159265359;
  float3 hdr=max(color.rgb*(original_ambient_light(color.a,lighting.x)*lighting.y+emission+direct),0);
  scene[id.xy]=float4(apply_scene_fog(hdr,position.xyz,lighting.x,lighting.z,lighting.w>0,lighting.w),1);
}
