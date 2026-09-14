#ifndef OCTARYN_DDGI_TYPES
#define OCTARYN_DDGI_TYPES
struct DDGIControl {int3 cell;uint version;uint refreshFrame;uint3 padding;};
struct DDGIProbe {
    float4 offset; // xyz relocation, w: active=0, inactive=1, sleeping=2
    uint4 metadata; // version, last traced frame, stable update count, valid
};
struct DDGIRay {float4 radianceDistance;float4 directionHit;};
uniform uint4 ddgiGrid; // xyz counts, enabled
uniform int4 ddgiOrigin; // xyz logical minimum
uniform float4 ddgiParameters; // spacing, hysteresis, maximum distance, normal bias
uniform uint4 ddgiFrame; // frame, rays per probe, irradiance resolution, distance resolution
uint ddgi_index(int3 cell) {
    int3 size=int3(ddgiGrid.xyz),wrapped=(cell%size+size)%size;
    return uint(wrapped.x+size.x*(wrapped.y+size.y*wrapped.z));
}
float2 ddgi_sign(float2 value) {return select(value>=0,float2(1),float2(-1));}
float2 ddgi_oct_encode(float3 direction) {
    direction/=max(dot(abs(direction),float3(1)),1e-8);
    float2 uv=direction.xy;
    if(direction.z<0)uv=(1-abs(uv.yx))*ddgi_sign(uv);
    return uv*.5+.5;
}
float3 ddgi_oct_decode(float2 uv) {
    float2 f=uv*2-1;float3 n=float3(f,1-abs(f.x)-abs(f.y));
    if(n.z<0)n.xy=(1-abs(n.yx))*ddgi_sign(n.xy);
    return normalize(n);
}
// Fold the octahedron at texture boundaries, preserving its spherical seams.
uint ddgi_texel(int2 xy,uint resolution) {
    int n=int(resolution);
    if(xy.x<0) {xy.x=-xy.x-1;xy.y=n-1-xy.y;}
    if(xy.x>=n) {xy.x=2*n-1-xy.x;xy.y=n-1-xy.y;}
    if(xy.y<0) {xy.y=-xy.y-1;xy.x=n-1-xy.x;}
    if(xy.y>=n) {xy.y=2*n-1-xy.y;xy.x=n-1-xy.x;}
    xy=clamp(xy,0,n-1);return uint(xy.x+n*xy.y);
}
uint ddgi_fixed_rays() {return min(32u,ddgiFrame.y/4);}
float3 ddgi_direction(uint ray,uint count,uint frame,uint probe) {
    uint fixedCount=ddgi_fixed_rays();
    bool fixedRay=ray<fixedCount;
    ray=fixedRay?ray:ray-fixedCount;count=fixedRay?fixedCount:count-fixedCount;
    float y=1-2*(float(ray)+.5)/float(count);
    float angle=6.28318530718*frac(float(ray)*.61803398875+float(((fixedRay?0:frame*97)+probe*17)%4096)/4096.0);
    float radius=sqrt(max(0,1-y*y));
    float3 direction=float3(cos(angle)*radius,y,sin(angle)*radius);
    // A second rotation changes the polar sampling distribution each frame.
    float rotation=fixedRay?0:6.28318530718*frac(float(frame)*.754877666);
    return float3(direction.x,direction.y*cos(rotation)-direction.z*sin(rotation),
        direction.y*sin(rotation)+direction.z*cos(rotation));
}
#endif
