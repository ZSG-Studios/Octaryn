#ifndef OCTARYN_ZERO_TANGENT_PLANE
#define OCTARYN_ZERO_TANGENT_PLANE
// Normalize the light/half-vector after projection into T=B=0, N.
// A zero projection remains zero; no replacement tangent frame is generated.
float zero_plane_cosine(float3 normal,float3 direction,float sampleZ) {
    return sampleZ*sign(dot(normal,direction));
}
float4 zero_plane_angles(float3 normal,float3 view,float3 light,float sampleZ) {
    float3 h=view+light;h/=max(length(h),1e-6);
    float v=sign(dot(normal,view)),halfway=sign(dot(normal,h));
    return saturate(float4(zero_plane_cosine(normal,light,sampleZ),sampleZ*v,sampleZ*halfway,v*halfway));
}
#endif
