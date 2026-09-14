#ifndef OCTARYN_LOCAL_LIGHTING_SLANG
#define OCTARYN_LOCAL_LIGHTING_SLANG
#include "LocalLight.slang"
#include "Surface.slang"
struct LocalSurface {float3 position;uint voxel;float3 normal;float valid;float3 color;float roughness;float metallic;float f0;float2 pad;};
Texture2D<float4> positions,voxels,colors,materials;
uniform float4 eye;
uniform uint4 extent; // width, height, frame, light count
LocalSurface local_surface(uint2 pixel) {
    LocalSurface s=(LocalSurface)0;
    float4 p=positions[pixel],m=materials[pixel];
    s.position=p.xyz+eye.xyz;s.valid=p.w>0?1:0;s.voxel=lighting_voxel(voxels[pixel]);
    s.normal=lighting_normal(s.voxel);s.color=colors[pixel].rgb;
    s.roughness=max(m.x,.02);s.metallic=m.y;s.f0=m.z;return s;
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
#endif
