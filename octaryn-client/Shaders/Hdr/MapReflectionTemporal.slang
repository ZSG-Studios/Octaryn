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
RWTexture2D<float4> reflectionHistory;
RWTexture2D<float> reflectionPosition;
RWTexture2D<float4> reflectionSurface;
RWTexture2D<float4> reflectionMaterial;
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
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=uint2(dimensions.xy)))return;
    uint2 sourcePixel=min(uint2((float2(id.xy)+.5)*sourceDimensions.xy/dimensions.xy),
        uint2(sourceDimensions.xy)-1);
    float4 position=positions[sourcePixel];
    uint voxel=lighting_voxel(voxels[sourcePixel]);
    if(position.w<=0 || !lighting_is_map(voxel)) {
        reflectionHistory[id.xy]=0;reflectionPosition[id.xy]=0;reflectionSurface[id.xy]=0;reflectionMaterial[id.xy]=0;return;
    }
    float3 world=position.xyz+eye.xyz,normal=lighting_normal(voxel);
    float3 view=-position.xyz/max(length(position.xyz),1e-5);
    float3 albedo=colors[sourcePixel].rgb;
    float metallic=saturate(materials[sourcePixel].g),roughness=saturate(materials[sourcePixel].r);
    uint sampleCount=map_reflection_sample_count(roughness);
    uint freshCount=min(sampleCount,uint(reflectionSampling.y));
    uint sampleIndex=(uint(dimensions.z)*freshCount)%sampleCount;
    float3 sample=0;
    for(uint i=0;i<freshCount;++i)
        sample+=map_reflection_sample(world,normal,view,albedo,metallic,roughness,
            sun,float4(lighting.xyz,0),(sampleIndex+i)%sampleCount,sampleCount);
    sample/=float(freshCount);
    float3 value=sample;
    float historyCount=1;
    bool historyMatch=false;
    if(dimensions.w!=0) {
        float3 relative=world-previousPositionCamera.xyz;
        float z=dot(relative,previousForward.xyz);
        float2 ndc=float2(dot(relative,previousRight.xyz),dot(relative,previousUp.xyz))*
            previousProjection.xy/max(z,1e-6)+previousJitter.xy;
        int2 pixel=int2(floor((ndc*float2(.5,-.5)+.5)*dimensions.xy));
        if(z>0 && all(pixel>=0) && all(pixel<int2(dimensions.xy))) {
            float oldDepth=previousPosition[pixel];
            float4 oldSurface=previousSurface[pixel];
            float4 oldMaterial=previousMaterial[pixel];
            float4 history=previousRadiance[pixel];
            // A nearest-pixel reprojection can move by half a pixel at native
            // 540p. Scale the world-space tolerance to that footprint while
            // keeping normal/material tests as the disocclusion guard.
            float pixelFootprint=z/max(previousProjection.y,1e-6)*2/dimensions.y;
            float tolerance=max(.02,max(.001*length(position.xyz),.75*pixelFootprint));
            float2 oldNdc=(float2(pixel)+.5)/dimensions.xy*2-float2(1,1);
            oldNdc.y=-oldNdc.y;
            float3 previousRay=normalize(previousRight.xyz*((oldNdc.x-previousJitter.x)/
                max(previousProjection.x,1e-6))+previousUp.xyz*((oldNdc.y-previousJitter.y)/
                max(previousProjection.y,1e-6))+previousForward.xyz);
            float3 oldWorld=previousPositionCamera.xyz+
                previousRight.xyz*((oldNdc.x-previousJitter.x)*oldDepth/max(previousProjection.x,1e-6))+
                previousUp.xyz*((oldNdc.y-previousJitter.y)*oldDepth/max(previousProjection.y,1e-6))+
                previousForward.xyz*oldDepth;
            float3 currentView=normalize(eye.xyz-world);
            float3 previousView=normalize(previousPositionCamera.xyz-oldWorld);
            // Glossy reflections are view-dependent even when the receiver matches.
            float viewTolerance=lerp(.00005,.005,roughness*roughness);
            bool matches=oldDepth>0 && history.a>0 &&
                shadow_history_position(oldWorld,world,voxel,previousPositionCamera.xyz,previousRay,tolerance) &&
                dot(oldSurface.xyz,normal)>=.98 &&
                abs(oldSurface.w-roughness)<=.03 && dot(currentView,previousView)>=1-viewTolerance &&
                distance(oldMaterial.rgb,albedo)<=.035 && abs(oldMaterial.a-metallic)<=.035;
            if(matches) {
                // Smooth lobes respond quickly; rough stable lobes can safely
                // amortize more samples without smearing sharp reflected edges.
                float limit=lerp(4,reflectionSampling.z,smoothstep(.08,.5,roughness));
                if(distance(eye.xyz,previousPositionCamera.xyz)>1e-5)limit=min(limit,8);
                historyCount=min(history.a+1,limit);
                value=lerp(sample,history.rgb,(historyCount-1)/historyCount);
                historyMatch=true;
            }
        }
    }
    if(!historyMatch && sampleCount>freshCount) {
        // Disocclusions use the exact stable quadrature this path replaces.
        // Only rejected pixels pay for the additional rays.
        float3 resolved=sample*float(freshCount);
        for(uint i=freshCount;i<sampleCount;++i)
            resolved+=map_reflection_sample(world,normal,view,albedo,metallic,roughness,
                sun,float4(lighting.xyz,0),(sampleIndex+i)%sampleCount,sampleCount);
        value=resolved/float(sampleCount);
    }
    reflectionHistory[id.xy]=float4(value,historyCount);
    reflectionPosition[id.xy]=dot(world-eye.xyz,currentForward.xyz);
    reflectionSurface[id.xy]=float4(normal,roughness);
    reflectionMaterial[id.xy]=float4(albedo,metallic);
}
