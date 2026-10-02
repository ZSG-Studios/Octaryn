#ifndef OCTARYN_SCENE_ENVIRONMENT_SLANG
#define OCTARYN_SCENE_ENVIRONMENT_SLANG
uniform float4 sceneAmbient; // RGB ambient radiance and authored environment enabled.
uniform float4 sceneDirectional; // RGB radiance and sky enabled.
uniform float4 sceneBackground;
float3 scene_direct_color() {return sceneAmbient.w>0?sceneDirectional.rgb:float3(1);}
float3 scene_diffuse_ambient(float3 normal,float3 direction,float4 sky,float visibility) {
    return sceneAmbient.w>0?sceneAmbient.rgb:diffuse_environment(normal,direction,sky)*visibility;
}
float3 scene_miss_radiance(float3 direction,float3 sun,float4 sky) {
    return sceneAmbient.w>0 && sceneDirectional.w==0?sceneBackground.rgb:diffuse_environment(direction,sun,sky);
}
#endif
