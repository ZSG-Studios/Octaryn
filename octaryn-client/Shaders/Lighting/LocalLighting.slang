#ifndef OCTARYN_LOCAL_LIGHTING_SLANG
#define OCTARYN_LOCAL_LIGHTING_SLANG
#include "Reservoir.slang"
#include "LocalLight.slang"
#include "Surface.slang"
struct LocalSurface {float3 position;uint voxel;float3 normal;float valid;float3 color;float roughness;float metallic;float f0;float2 pad;};
struct LocalHistory {float3 position;uint voxel;float3 normal;float valid;};
Texture2D<float4> positions,voxels,colors,materials;
uniform float4 eye;
uniform uint4 extent; // width, height, frame, light count
uniform float4 reuseSettings; // position tolerance, normal cosine, radius, history cap
uniform uint4 restirSettings; // candidates, neighbors, history valid, debug
LocalSurface local_surface(uint2 pixel) {
    LocalSurface s=(LocalSurface)0;
    float4 p=positions[pixel],m=materials[pixel];
    s.position=p.xyz+eye.xyz;s.valid=p.w>0?1:0;s.voxel=lighting_voxel(voxels[pixel]);
    s.normal=lighting_normal(s.voxel);s.color=colors[pixel].rgb;
    s.roughness=max(m.x,.02);s.metallic=m.y;s.f0=m.z;return s;
}
LocalHistory local_history(LocalSurface s) {
    LocalHistory h;h.position=s.position;h.voxel=s.voxel;h.normal=s.normal;h.valid=s.valid;return h;
}
bool compatible_history(LocalSurface a,LocalHistory b) {
    return a.valid>0 && b.valid>0 && a.voxel==b.voxel && dot(a.normal,b.normal)>=reuseSettings.y && length(a.position-b.position)<reuseSettings.x;
}
bool compatible_surface(LocalSurface a,LocalSurface b,bool temporal) {
    if(a.valid==0 || b.valid==0 || a.voxel!=b.voxel || dot(a.normal,b.normal)<reuseSettings.y)return false;
    float3 delta=a.position-b.position;
    return temporal?length(delta)<reuseSettings.x:abs(dot(delta,a.normal))<reuseSettings.x;
}
float3 local_brdf(LocalSurface s,float3 L) {
    float3 N=s.normal,V=normalize(eye.xyz-s.position);
    if((s.voxel&256)!=0 && dot(N,V)<0)N=-N;
    float nl=saturate(dot(N,L)),nv=max(dot(N,V),1e-4);
    float3 halfVector=V+L;
    float3 H=halfVector/max(length(halfVector),1e-6);
    float nh=saturate(dot(N,H)),vh=saturate(dot(V,H));
    float alpha=s.roughness*s.roughness,a2=alpha*alpha;
    float d=a2/(3.141592654*pow(nh*nh*(a2-1)+1,2));
    float k=pow(s.roughness+1,2)/8;
    float g=(nv/(nv*(1-k)+k))*(nl/max(nl*(1-k)+k,1e-5));
    float3 f0=lerp(float3(s.f0),s.color,s.metallic);
    float3 f=f0+(1-f0)*pow(1-vh,5);
    return ((1-f)*(1-s.metallic)*s.color/3.141592654+f*d*g/max(4*nv*nl,1e-5))*nl;
}
float3 local_contribution(LocalSurface s,uint lightId,float2 sample,out float3 L,out float distance) {
    float3 radiance=local_light_radiance(s.position,lightId,sample,L,distance);
    return local_brdf(s,L)*radiance;
}
float local_target(LocalSurface s,uint lightId,float2 sample) {
    float3 L;float distance;
    float3 value=local_contribution(s,lightId,sample,L,distance);
    // A strictly positive target gives every light common support at every surface.
    // This avoids zero-support reuse bias without extra visibility rays per source.
    return max(dot(value,float3(.2126,.7152,.0722)),1e-8);
}
#endif
