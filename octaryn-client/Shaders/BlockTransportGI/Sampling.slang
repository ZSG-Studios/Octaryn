#ifndef OCTARYN_BLOCK_TRANSPORT_SAMPLING
#define OCTARYN_BLOCK_TRANSPORT_SAMPLING
#include "Types.slang"
#include "PlantMask.slang"
#include "CubeMask.slang"
#include "Sequence.slang"
#include "SurfaceOrigin.slang"
uniform float4 btCoverageMin,btCoverageMax;
bool bt_inside(float3 position) {
    return btCoverageMin.w!=0 && all(position>=btCoverageMin.xyz-.001) &&
        all(position<=btCoverageMax.xyz+.001);
}
float bt_sky_distance(float3 origin,float3 direction) {
    if(!bt_inside(origin) || direction.y<=1e-6)return 0;
    float distance=(btCoverageMax.y-origin.y)/direction.y;
    float3 endpoint=origin+direction*distance;
    if(distance<=0 || any(endpoint.xz<btCoverageMin.xz) || any(endpoint.xz>btCoverageMax.xz))return 0;
    return distance+.002;
}
bool bt_direct_position_domain(int4 key,float domain,uint batch,inout uint random,out float3 position) {
    if(world_plant_tag(uint(key.w)))return bt_plant_position(key,random,position);
    float2 sample=bt_sequence(batch,bt_key_hash(key)^0x68bc21ebu).xy;
    if(domain<0)return bt_cube_mask_position(key,domain,sample,position);
    return bt_cube_position(key,sample,position);
}
bool bt_sample_domain(int4 key,float domain,uint slot,uint batch,out float3 origin,out float3 direction) {
    uint random=bt_hash(bt_key_hash(key)^bt_hash(slot+1)^bt_hash(batch+0xa511e9b3u));
    float4 sample=bt_transport_sequence(key,slot,batch);
    float3 normal=bt_normal(key.w);
    bool plant=world_plant_tag(uint(key.w));
    float3 tangent=plant?float3(0,1,0):key.w<2?float3(0,0,1):float3(1,0,0);
    float3 bitangent=cross(normal,tangent);
    if(!plant && domain<0) {
        if(!bt_cube_mask_position(key,domain,sample.xy,origin)) {direction=0;return false;}
    } else if(plant) {
        if(!bt_plant_position(key,random,origin)) {direction=0;return false;}
    } else if(!bt_cube_position(key,sample.xy,origin)) {direction=0;return false;}
    float3 shifted;
    if(!bt_outward_origin(origin,normal,shifted)) {direction=0;return false;}
    origin=shifted;
    float radius=sqrt(sample.z),angle=sample.w*6.28318530718;
    direction=tangent*(radius*cos(angle))+bitangent*(radius*sin(angle))+normal*sqrt(1-sample.z);
    return true;
}
bool bt_direct_position(int4 key,uint batch,inout uint random,out float3 position) {
    return bt_direct_position_domain(key,0,batch,random,position);
}
bool bt_sample(int4 key,uint slot,uint batch,out float3 origin,out float3 direction) {
    return bt_sample_domain(key,0,slot,batch,origin,direction);
}
bool bt_surface_position(BtSurface surface,uint batch,inout uint random,out float3 position) {
    return bt_direct_position_domain(surface.key,surface.albedo.w,batch,random,position);
}
bool bt_surface_sample(BtSurface surface,uint slot,uint batch,out float3 origin,out float3 direction) {
    return bt_sample_domain(surface.key,surface.albedo.w,slot,batch,origin,direction);
}
#endif
