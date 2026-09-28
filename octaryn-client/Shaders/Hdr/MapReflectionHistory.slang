// Uses the temporal resolve's bound histories and previous camera uniforms.
float2 reflection_history_ndc(int2 pixel) {
    uint2 sourcePixel=min(uint2((float2(pixel)+.5)*sourceDimensions.xy/dimensions.xy),
        uint2(sourceDimensions.xy)-1);
    return ((float2(sourcePixel)+.5)/sourceDimensions.xy*2-1)*float2(1,-1);
}
bool reflection_history_candidate(int2 pixel,float3 world,float3 normal,uint voxel,
    float roughness,float metallic,float3 albedo,float tolerance) {
    if(any(pixel<0) || any(pixel>=int2(dimensions.xy)))return false;
    float oldDepth=previousPosition[pixel];
    float4 oldSurface=previousSurface[pixel],oldMaterial=previousMaterial[pixel];
    float4 history=previousRadiance[pixel];
    if(history.a<0)ray_diagnostic(10);
    float2 oldNdc=reflection_history_ndc(pixel);
    float3 previousRay=normalize(previousRight.xyz*((oldNdc.x-previousJitter.x)/
        max(previousProjection.x,1e-6))+previousUp.xyz*((oldNdc.y-previousJitter.y)/
        max(previousProjection.y,1e-6))+previousForward.xyz);
    float3 oldWorld=previousPositionCamera.xyz+
        previousRight.xyz*((oldNdc.x-previousJitter.x)*oldDepth/max(previousProjection.x,1e-6))+
        previousUp.xyz*((oldNdc.y-previousJitter.y)*oldDepth/max(previousProjection.y,1e-6))+
        previousForward.xyz*oldDepth;
    float3 currentView=normalize(eye.xyz-world);
    float3 previousView=normalize(previousPositionCamera.xyz-oldWorld);
    float viewTolerance=lerp(.00005,.005,roughness*roughness);
    return oldDepth>0 && history.a>0 &&
        shadow_history_position(oldWorld,world,voxel,previousPositionCamera.xyz,previousRay,tolerance) &&
        dot(oldSurface.xyz,normal)>=.98 && abs(oldSurface.w-roughness)<=.03 &&
        dot(currentView,previousView)>=1-viewTolerance &&
        distance(oldMaterial.rgb,albedo)<=.035 && abs(oldMaterial.a-metallic)<=.035;
}
bool reflection_history_pixel(float3 world,float3 normal,uint voxel,float roughness,
    float metallic,float3 albedo,float viewDistance,out int2 selected) {
    selected=int2(-1);
    float3 relative=world-previousPositionCamera.xyz;
    float z=dot(relative,previousForward.xyz);
    if(z<=0)return false;
    float2 ndc=float2(dot(relative,previousRight.xyz),dot(relative,previousUp.xyz))*
        previousProjection.xy/max(z,1e-6)+previousJitter.xy;
    float2 coordinate=(ndc*float2(.5,-.5)+.5)*dimensions.xy;
    float pixelFootprint=z/max(previousProjection.y,1e-6)*2/dimensions.y;
    float tolerance=max(.02,max(.001*viewDistance,.75*pixelFootprint));
    if(historySearch==0 || referenceMode!=0) {
        int2 pixel=int2(floor(coordinate));
        if(!reflection_history_candidate(pixel,world,normal,voxel,roughness,metallic,albedo,tolerance))return false;
        selected=pixel;return true;
    }
    int2 base=int2(floor(coordinate-.5));
    float best=1e30;
    [unroll]
    for(uint i=0;i<4u;++i) {
        int2 pixel=base+int2(i&1u,i>>1u);
        if(!reflection_history_candidate(pixel,world,normal,voxel,roughness,metallic,albedo,tolerance))continue;
        float2 delta=(reflection_history_ndc(pixel)-ndc)*sourceDimensions.xy;
        float score=dot(delta,delta);
        if(score<best){best=score;selected=pixel;}
    }
    return selected.x>=0;
}
