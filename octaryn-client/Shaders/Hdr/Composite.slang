// Original shaders/composite.comp.glsl: sky inverse tone map, ambient and emission.
#include "../Materials/PbrEvaluate.slang"
#include "../Sky/Fog.slang"
#include "../Lighting/Surface.slang"
#ifdef WORLD_BLOCK_TRANSPORT
#include "../BlockTransportGI/Reconstruct.slang"
Texture2D<uint4> blockSurfaceKeys;
#endif
#include "../Materials/PbrEnvironment.slang"
#include "../Lighting/DiffuseEnvironment.slang"
#include "../Lighting/SceneEnvironment.slang"
#ifdef OCTARYN_COMPOSITE_RAY_SKY
#include "MapReflections.slang"
Texture2D<float4> temporalReflections;
Texture2D<float> temporalReflectionDepth;
Texture2D<float4> temporalReflectionSurface;
uniform uint useTemporalReflections;
uniform float4 reflectionDimensions;
#endif
Texture2D<float4> colors;
Texture2D<float4> positions;
Texture2D<float4> voxels;
Texture2D<float4> materials;
Texture2D<float4> emissive;
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
    // The sky pass writes linear HDR directly; no LDR round trip.
    scene[id.xy]=float4(max(sceneAmbient.w>0 && sceneDirectional.w==0?sceneBackground.rgb:color.rgb,0),1);return;
  }
  float emission=clamp(materials[id.xy].a,0,8);
  uint4 packed=uint4(round(voxels[id.xy]*255));
  uint voxel=packed.x|(packed.y<<8)|(packed.z<<16)|(packed.w<<24);
  if(lighting_is_unlit(voxel,materials[id.xy]) && dimensions.w==0) {
    scene[id.xy]=float4(apply_scene_fog(color.rgb,position.xyz,lighting.x,lighting.z,lighting.w>0,lighting.w),1);return;
  }
  float nl=dot(lighting_normal(voxel),sun.xyz);
  bool sprite=!lighting_is_map(voxel) && (voxel&256)!=0;
  if(sprite && !world_plant_voxel(voxel))nl=abs(nl);
  float visibility=dimensions.z!=0?sunVisibility[id.xy]:1;
  float direct=max(nl,0)*sun.w*visibility/3.14159265359;
  float3 geometric=lighting_normal(voxel);
  float3 normal=geometric;
  if(sprite && dot(normal,-position.xyz)<0)normal=-normal;
  uint debug=uint(dimensions.w);
  bool diffuseReceiver=!lighting_is_map(voxel) || materials[id.xy].g<1 || debug!=0;
  float3 indirect=0;
#ifdef WORLD_BLOCK_TRANSPORT
  if(diffuseReceiver)reconstruct_block_transport_gi(blockSurfaceKeys[id.xy],
      float2(color.a,emissive[id.xy].a),indirect);
#endif
  float3 hdr=max(color.rgb*((indirect+direct)*(1-materials[id.xy].g)+emission)+localLighting[id.xy].rgb,0);
  if(lighting_is_map(voxel)) {
    float3 view=-position.xyz/max(length(position.xyz),1e-5);
    float metallic=saturate(materials[id.xy].g),roughness=saturate(materials[id.xy].r);
    float3 fresnel=pbr_environment_reflectance(lerp(float3(.04),color.rgb,metallic),roughness,dot(normal,view));
    float occlusion=float((voxel>>8)&255)/255;
    // Sky-dome diffuse ambient fills shadowed map surfaces; color.a carries the
    // hemisphere ambient visibility written by the map G-buffer pass.
    float3 ambient=scene_diffuse_ambient(normal,sun.xyz,float4(lighting.xyz,0),color.a)*occlusion;
    float3 directMap=lighting_is_zero_basis(voxel,materials[id.xy])?
        pbr_direct_zero_plane(color.rgb,metallic,roughness,normal,view,sun.xyz,lighting_zero_basis_z(materials[id.xy])):
        pbr_direct(color.rgb,metallic,roughness,normal,view,sun.xyz);
    hdr=color.rgb*(1-metallic)*(1-fresnel)*(indirect*occlusion+ambient)+emissive[id.xy].rgb+localLighting[id.xy].rgb+
        directMap*sun.w*visibility*scene_direct_color();
#ifdef OCTARYN_COMPOSITE_RAY_SKY
    if(useTemporalReflections!=0 && all(reflectionDimensions.xy==dimensions.xy)) {
      // Native-rate reflections already share this exact G-buffer receiver.
      hdr+=temporalReflections[id.xy].rgb;
    } else if(useTemporalReflections!=0) {
      float2 uv=(float2(id.xy)+.5)/dimensions.xy*reflectionDimensions.xy-float2(.5);
      int2 base=int2(floor(uv));float2 f=frac(uv);
      float3 sum=0;float total=0;float centerDepth=position.w;
      for(int oy=0;oy<2;++oy)for(int ox=0;ox<2;++ox) {
        int2 samplePixel=clamp(base+int2(ox,oy),int2(0),int2(reflectionDimensions.xy)-1);
        float4 sampleSurface=temporalReflectionSurface[samplePixel];
        float normalWeight=saturate(dot(sampleSurface.xyz,normal));
        float depthWeight=exp(-abs(temporalReflectionDepth[samplePixel]-centerDepth)*24);
        float bilinear=((ox==0)?1-f.x:f.x)*((oy==0)?1-f.y:f.y);
        float weight=bilinear*normalWeight*normalWeight*depthWeight;
        sum+=temporalReflections[samplePixel].rgb*weight;total+=weight;
      }
      hdr+=total>1e-4?sum/total:temporalReflections[clamp(int2(id.xy*reflectionDimensions.xy/dimensions.xy),int2(0),int2(reflectionDimensions.xy)-1)].rgb;
    } else {
      hdr+=map_reflections(position.xyz+eye.xyz,normal,view,color.rgb,metallic,roughness,sun,float4(lighting.xyz,0));
    }
#endif
  }
  if(debug==1) {scene[id.xy]=float4(visibility.xxx,1);return;}
  if(debug==8) {scene[id.xy]=float4(localLighting[id.xy].rgb,1);return;}
  if(debug==12) {scene[id.xy]=float4(sunHistory[id.xy].y/32,visibility,0,1);return;}
  if(debug==28) {scene[id.xy]=float4(color.rgb*direct*(1-materials[id.xy].g),1);return;}
  if(debug==31) {
    // Leak localization: red = direct sun term (4x), green = raw sun dot,
    // blue = filtered visibility. Sprites/foliage pixels with green+blue lit
    // but red dark, or all three lit, name the leaking term directly.
    scene[id.xy]=float4(direct*4,saturate(nl)*.5+.5,visibility,1);return;
  }
  if(debug>=13 && debug<=20) {scene[id.xy]=float4(localLighting[id.xy].rgb,1);return;}
  scene[id.xy]=float4(apply_scene_fog(hdr,position.xyz,lighting.x,lighting.z,lighting.w>0,lighting.w),1);
}
