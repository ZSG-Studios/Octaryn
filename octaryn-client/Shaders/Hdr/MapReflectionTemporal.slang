#include "../Lighting/DiffuseEnvironment.slang"
#include "../Lighting/SceneEnvironment.slang"
#include "../Lighting/Surface.slang"
#include "../Shadows/Reprojection.slang"
#include "MapReflections.slang"
Texture2D<float4> colors;
Texture2D<float4> positions;
Texture2D<float4> voxels;
Texture2D<float4> materials;
Texture2D<float4> previousRadiance;
Texture2D<float> previousPosition;
Texture2D<float4> previousSurface;
Texture2D<float4> previousMaterial;
Texture2D<float2> previousMoments;
RWTexture2D<float4> reflectionHistory;
RWTexture2D<float> reflectionPosition;
RWTexture2D<float4> reflectionSurface;
RWTexture2D<float4> reflectionMaterial;
RWTexture2D<float2> reflectionMoments;
uniform uint referenceMode;
uniform uint fullFreshDiagnostic;
uniform uint sparseMode;
uniform uint historySearch;
Texture2D<uint> reflectionTiles;
uniform float4 eye;
uniform float4 previousPositionCamera;
uniform float4 previousRight;
uniform float4 previousUp;
uniform float4 previousForward;
uniform float4 previousProjection;
uniform float4 previousJitter;
uniform float4 currentForward;
uniform float4 sun;
uniform float4 lighting;
uniform float4 dimensions; // width, height, frame index, history valid
uniform float4 sourceDimensions; // full scene shading extent
#include "MapReflectionHistory.slang"
[shader("compute")]
[numthreads(8,8,1)]
#include "ReflectionWaveAttributes.slang"
void main(uint3 id:SV_DispatchThreadID) {
    ray_diagnostic_wave();
    if(any(id.xy>=uint2(dimensions.xy)))return;
    uint2 sourcePixel=min(uint2((float2(id.xy)+.5)*sourceDimensions.xy/dimensions.xy),
        uint2(sourceDimensions.xy)-1);
    float4 position=positions[sourcePixel];
    uint voxel=lighting_voxel(voxels[sourcePixel]);
    if(position.w<=0 || !lighting_is_map(voxel) || lighting_is_unlit(voxel,materials[sourcePixel])) {
        reflectionHistory[id.xy]=0;reflectionPosition[id.xy]=0;reflectionSurface[id.xy]=0;reflectionMaterial[id.xy]=0;reflectionMoments[id.xy]=0;return;
    }
    ray_diagnostic(4);
    float3 world=position.xyz+eye.xyz,normal=lighting_normal(voxel);
    float3 view=-position.xyz/max(length(position.xyz),1e-5);
    float3 albedo=colors[sourcePixel].rgb;
    float metallic=saturate(materials[sourcePixel].g),roughness=saturate(materials[sourcePixel].r);
    uint sampleCount=map_reflection_sample_count(roughness);
    uint freshCount=min(sampleCount,uint(reflectionSampling.y));
    float3 inherited=0;
    float2 oldMoments=0;
    float historyCount=1;
    float inheritedCount=0;
    bool historyMatch=false;
    int2 historyPixel;
    if(dimensions.w!=0 && reflection_history_pixel(world,normal,voxel,roughness,metallic,albedo,
        length(position.xyz),historyPixel)) {
        float4 history=previousRadiance[historyPixel];
        float limit=lerp(4,reflectionSampling.z,smoothstep(.08,.5,roughness));
        if(distance(eye.xyz,previousPositionCamera.xyz)>1e-5)limit=min(limit,8);
        historyCount=min(history.a+1,limit);
        inherited=history.rgb;inheritedCount=history.a;
        oldMoments=previousMoments[historyPixel];historyMatch=true;
    }
    if(!historyMatch)ray_diagnostic(5);
    // While the ray scene builds, zero radiance is absent data, not black
    // geometry. Hold matched history and its age unchanged, mark unmatched
    // pixels invalid, and let the first usable frame integrate real samples.
    if(raySettings.x<.5) {
        if(historyMatch) {
            reflectionHistory[id.xy]=float4(inherited,inheritedCount);
            reflectionMoments[id.xy]=oldMoments;
        } else {reflectionHistory[id.xy]=0;reflectionMoments[id.xy]=0;}
        reflectionPosition[id.xy]=dot(world-eye.xyz,currentForward.xyz);
        reflectionSurface[id.xy]=float4(normal,roughness);
        reflectionMaterial[id.xy]=float4(albedo,metallic);
        return;
    }
    if(referenceMode!=0) {
        if(!historyMatch)freshCount=sampleCount;
    } else {
        float variance=max(0,oldMoments.y-oldMoments.x*oldMoments.x);
        float relativeVariance=variance/max(.01,oldMoments.x*oldMoments.x);
        // Sharp lobes stay full rate. Stable rough lobes amortize directions;
        // newly visible or noisy surfaces recover with at most two fresh rays.
        freshCount=roughness>=.25 && historyMatch && historyCount>=8 && relativeVariance<.1?
            1u:min(sampleCount,2u);
    }
    // Diagnostic control: isolate temporal reuse from fresh-direction density.
    if(fullFreshDiagnostic!=0)freshCount=sampleCount;
    uint sampleIndex=(uint(dimensions.z)*(referenceMode!=0?uint(reflectionSampling.y):1u))%sampleCount;
    if(fullFreshDiagnostic!=0)sampleIndex=0;
    // Refresh alternate rough receivers each frame; sharp or newly exposed
    // surfaces always trace. Reused samples do not earn additional confidence.
    bool reuse=fullFreshDiagnostic==0 && sparseMode!=0 && referenceMode==0 && mapDynamicInstanceStart>=0x800000u && historyMatch && historyCount>=16 &&
        roughness>=.45 && reflectionTiles[id.xy/8]!=0 &&
        ((id.x+id.y+uint(dimensions.z))&1u)!=0;
    if(reuse) {
        reflectionHistory[id.xy]=float4(inherited,inheritedCount);
        reflectionMoments[id.xy]=oldMoments;
        reflectionPosition[id.xy]=dot(world-eye.xyz,currentForward.xyz);
        reflectionSurface[id.xy]=float4(normal,roughness);
        reflectionMaterial[id.xy]=float4(albedo,metallic);
        return;
    }
    float3 sample=0;
    bool dynamic=false;
    for(uint i=0;i<freshCount;++i)
        sample+=map_reflection_sample(world,normal,view,albedo,metallic,roughness,
            sun,float4(lighting.xyz,0),(sampleIndex+i)%sampleCount,sampleCount,dynamic);
    sample/=float(freshCount);
    float weight=historyMatch && !dynamic?(historyCount-1)/historyCount:0;
    float3 value=lerp(sample,inherited,weight);
    float luminance=dot(sample,float3(.2126,.7152,.0722));
    // Store normalized moments to keep HDR radiance within half-float range.
    float bounded=luminance/(1+luminance);
    reflectionMoments[id.xy]=lerp(float2(bounded,bounded*bounded),oldMoments,weight);
    reflectionHistory[id.xy]=float4(value,dynamic?-1:historyCount);
    if(dynamic)ray_diagnostic(11);
    reflectionPosition[id.xy]=dot(world-eye.xyz,currentForward.xyz);
    reflectionSurface[id.xy]=float4(normal,roughness);
    reflectionMaterial[id.xy]=float4(albedo,metallic);
}
