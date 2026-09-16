// Original transparent.frag.glsl: fluid tint/absorption, reflection, ripples and lava glow.
#include "../Sky/Fog.slang"
#ifdef OCTARYN_RAY_TRACING
#include "../RayTracing/WorldRayQuery.slang"
#include "../DDGI/DDGISample.slang"
uniform float reflectionRange; // Blocks; zero traces the full scene range.
#endif
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
    // Flatten ripples toward a mirror at grazing angles and with distance.
    // Per-pixel ripple swings the reflection between shore and sky, which reads
    // as flickering diamonds along the waterline and sparkle fields offshore.
    // Subpixel normal variation aliases hardest far away; near detail is kept.
    float facing=saturate(dot(geometric,view));
    float distanceDamp=exp(-viewDepth*.02);
    float3 ripple=normalize(float3((ns.rg*2-1)*(.06*facing*distanceDamp),1));
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
  // Sun glint from the static geometric normal: the animated ripple normal made
  // pow96 peaks travel and pop frame to frame (white-dot shimmer). Ripple still
  // drives fresnel and reflection tint, so the water stays alive.
  float3 specNormal=geometric;
  if(dot(specNormal,view)<0)specNormal=-specNormal;
  float specular=pow(saturate(dot(specNormal,halfDirection)),96)*saturate(lightDirectionSky.w*worldTime.z)*up*worldTime.w;
  float smoothness=.68;
  if(materialSettings.x!=0) smoothness=max(fluidSampleAtlas(atlasSpecular,uv,sampleVoxel).r,smoothness);
  specular*=lerp(.05,.20,smoothness);
  // Clamp HDR glint spikes: unfiltered pow96 peaks blow out to white dots that
  // pop frame to frame. Broad sun-path brightness is preserved below the cap.
  specular=min(specular,2.0);
#ifdef OCTARYN_RAY_TRACING
  if(specular>0 && raySettings.x!=0)
    specular*=world_ray_visibility(input.relative+cameraPosition.xyz,geometric,normalize(-lightDirectionSky.xyz));
#endif
  float3 absorption=lerp(float3(.004,.025,.15),float3(.015,.09,.38),fill);
  float3 lit=lerp(float3(.002,.006,.020),absorption,saturate(lightDirectionSky.w*.82+.18));
  float3 sideAbsorption=lerp(lit,float3(.010,.055,.25),.35);
  float3 color=lerp(albedo.rgb,side?sideAbsorption:lit,.42);
  float3 reflected=reflect(-view,normal);
#ifdef OCTARYN_RAY_TRACING
  float3 reflectionColor=get_sky_color_lit(reflected,lightDirectionSky.w,worldTime.y);
  if(raySettings.x!=0 && reflectionRange>0) {
    const float range=reflectionRange;
    WorldRayHit hit=world_trace_ray(input.relative+cameraPosition.xyz+geometric*raySettings.z,reflected,range);
    if(hit.hit) {
      float3 radiance;
      if((hit.flags&512)!=0) radiance=world_ray_emission(hit);
      else {
        // Direct sun plus DDGI diffuse bounce, matching the composite lighting
        // convention so reflections receive the same indirect light as the world.
        const float3 sunDirection=normalize(-lightDirectionSky.xyz);
        float direct=max(dot(hit.normal,sunDirection),0)*max(worldTime.w,0)/3.14159265359;
        if(direct>0)direct*=world_ray_visibility(hit.position,hit.normal,sunDirection);
        float coverage;
        const float3 indirect=ddgi_sample(hit.position,hit.normal,coverage)/3.14159265359;
        const float ambient=original_ambient_visibility(hit.normal,is_sprite_voxel(hit.voxel),
            materialSettings.z,lightDirectionSky.w);
        const float3 fallback=original_ambient_light(ambient,lightDirectionSky.w)*materialSettings.w;
        radiance=max(hit.albedo.rgb*(lerp(fallback,indirect,coverage)+direct)+world_ray_emission(hit),0);
      }
      // Forward water and atmosphere use the same bounded reflection domain.
      // Fade traced reflections back to the sky across the range edge.
      const float fade=1-saturate((hit.distance/range-.875)*8);
      reflectionColor=lerp(reflectionColor,radiance/(1+radiance),fade);
    }
  }
  color=lerp(color,reflectionColor,(.02+fresnel*.48)*up);
#else
  reflected.y=max(reflected.y,.03);
  color=lerp(color,get_sky_color_lit(reflected,lightDirectionSky.w,worldTime.y),(.015+fresnel*.07)*up);
#endif
  color+=float3(1,.92,.76)*specular;
  color=lerp(color,get_fog_color(float3(0,.14,1),lightDirectionSky.w,worldTime.y),distanceFog*.03);
  float alpha=lerp(.74,.88,fill);if(side) alpha*=.86;alpha=lerp(alpha,.96,fresnel*up);
  return float4(saturate(color),saturate(alpha));
}
