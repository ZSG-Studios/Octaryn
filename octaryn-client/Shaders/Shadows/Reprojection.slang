#ifndef OCTARYN_SHADOW_REPROJECTION
#define OCTARYN_SHADOW_REPROJECTION
#include "../Lighting/Surface.slang"

bool shadow_history_position(float3 old,float3 world,uint voxel,
    float3 previousEye,float3 previousRay,float tolerance) {
    // Sprite normals are lighting axes, not their geometric planes.
    if((voxel&256u)!=0)return distance(old,world)<tolerance;
    float3 normal=lighting_normal(voxel);
    float denominator=dot(previousRay,normal);
    if(abs(denominator)<1e-6)return false;
    float depth=dot(world-previousEye,normal)/denominator;
    if(depth<=0)return false;
    // Reprojection selects a pixel center, not the original subpixel point.
    float3 expected=previousEye+previousRay*depth;
    return distance(old,expected)<tolerance;
}
#endif
