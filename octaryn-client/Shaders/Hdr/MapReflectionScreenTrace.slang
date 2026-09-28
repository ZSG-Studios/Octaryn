// Screen data supplies a candidate surface, never primary-view radiance.
#include "ScreenGrid.slang"
bool reflection_screen_project(float3 relative,out float2 pixel,out float depth) {
    depth=dot(relative,currentForward.xyz);pixel=0;if(depth<=.05)return false;
    float2 ndc=float2(dot(relative,currentRight.xyz),dot(relative,currentUp.xyz))*currentProjection.xy/depth;
    pixel=((ndc+currentJitter.xy)*float2(.5,-.5)+.5)*sourceDimensions.xy;
    return all(pixel>=1) && all(pixel<sourceDimensions.xy-1);
}
bool reflection_screen_covered(int2 pixel) {
    return any(pixel<1) || any(pixel>=int2(sourceDimensions.xy)-1) || surfaceFlags[pixel]!=1u;
}
bool reflection_screen_plane(int2 pixel,out float3 normal,out float tolerance) {
    normal=0;tolerance=0;if(reflection_screen_covered(pixel))return false;
    float4 center=positions[pixel];if(center.w<=0)return false;
    float3 left=positions[pixel+int2(-1,0)].xyz,right=positions[pixel+int2(1,0)].xyz;
    float3 up=positions[pixel+int2(0,-1)].xyz,down=positions[pixel+int2(0,1)].xyz;
    float3 crossValue=cross(right-left,down-up);
    if(dot(crossValue,crossValue)<1e-12)return false;
    normal=normalize(crossValue);if(dot(normal,-center.xyz)<0)normal=-normal;
    float footprint=center.w*2/max(currentProjection.y*sourceDimensions.y,1);
    tolerance=max(.001,min(.01,footprint*.1));
    float3 shading=lighting_normal(lighting_voxel(voxels[pixel]));
    float4 material=materials[pixel];float3 albedo=colors[pixel].rgb;
    // Reject silhouettes, holes, unsupported projected coverage and material seams.
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
        int2 tap=pixel+int2(x,y);if(reflection_screen_covered(tap))return false;
        float4 other=positions[tap];uint voxel=lighting_voxel(voxels[tap]);
        if(other.w<=0 || !lighting_is_map(voxel) ||
            abs(dot(other.xyz-center.xyz,normal))>tolerance ||
            dot(shading,lighting_normal(voxel))<.995 ||
            any(abs(materials[tap].rg-material.rg)>.015) || distance(colors[tap].rgb,albedo)>.02)return false;
    }
    return true;
}
bool reflection_screen_segment_clear(int2 pixel) {
    // A one-pixel halo also covers nearly coincident DDA corners and grid edges.
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)
        if(reflection_screen_covered(pixel+int2(x,y)))return false;
    return true;
}
float reflection_screen_distance(float fraction,float z0,float z1,float maximum) {
    return maximum*(fraction/z1)/((1-fraction)/z0+fraction/z1);
}
bool reflection_screen_hit(float3 origin,float3 direction,out uint2 pixel,out float distance) {
    pixel=0;distance=0;
    float3 relative=origin-eye.xyz;float2 start;float z0;
    if(!reflection_screen_project(relative,start,z0))return false;
    float maximum=min(reflectionRange,20.f),dz=dot(direction,currentForward.xyz);
    if(dz<0)maximum=min(maximum,(z0-.06)/-dz);
    if(maximum<=.01)return false;
    float3 endpoint=relative+direction*maximum;float z1=dot(endpoint,currentForward.xyz);
    if(z1<=.05)return false;
    float2 ndc=float2(dot(endpoint,currentRight.xyz),dot(endpoint,currentUp.xyz))*currentProjection.xy/z1;
    float2 end=((ndc+currentJitter.xy)*float2(.5,-.5)+.5)*sourceDimensions.xy;
    float2 delta=end-start;if(max(abs(delta.x),abs(delta.y))<2)return false;
    ScreenGrid grid=screen_grid_begin(start,end);
    // Bounded supercover DDA: every crossed cell, including both corner neighbors.
    for(uint iteration=0;iteration<96;++iteration) {
        int2 cell=grid.cell;
        if(!reflection_screen_segment_clear(cell))return false;
        float finish=screen_grid_exit(grid);
        float lo=reflection_screen_distance(grid.entered,z0,z1,maximum);
        float hi=reflection_screen_distance(finish,z0,z1,maximum);
        float4 target=positions[cell];if(target.w<=0)return false;
        float endDepth=dot(relative+direction*hi,currentForward.xyz);
        float startDepth=dot(relative+direction*lo,currentForward.xyz);
        float footprint=target.w*2/max(currentProjection.y*sourceDimensions.y,1);
        if(max(startDepth,endDepth)>=target.w-footprint) {
            float3 normal;float tolerance;
            if(!reflection_screen_plane(cell,normal,tolerance))return false;
            float denominator=dot(direction,normal);
            if(denominator<-.05) {
                float candidate=dot(target.xyz-relative,normal)/denominator;
                if(candidate>=max(.005,lo) && candidate<=hi) {
                    float3 point=relative+direction*candidate;float2 projected;float depth;
                    if(!reflection_screen_project(point,projected,depth) || any(int2(projected)!=cell))return false;
                    if(length(point-target.xyz)>footprint*.8)return false;
                    pixel=uint2(cell);distance=candidate;return true;
                }
            }
            // Behind a primary surface without a validated crossing is unknown.
            if(min(startDepth,endDepth)>target.w+footprint)return false;
        }
        if(finish>=1)return false;
        screen_grid_advance(grid);
    }
    return false;
}
