#ifndef OCTARYN_BLOCK_TRANSPORT_RECONSTRUCT
#define OCTARYN_BLOCK_TRANSPORT_RECONSTRUCT
#include "Sample.slang"
bool bt_reconstruction_key(int4 key,int3 offset,out uint4 packed) {
    packed=0;
    for(uint axis=0;axis<3;++axis) {
        if((offset[axis]>0 && key[axis]==2147483647) ||
           (offset[axis]<0 && key[axis]==(-2147483647-1)))return false;
    }
    packed=uint4(asuint(key.xyz+offset),uint(key.w)+1);return true;
}
bool reconstruct_block_transport_gi(uint4 packed,float2 uv,out float3 value) {
    value=0;uint owner;
    if(!bt_lookup_surface(packed,owner))return false;
    bool complete;value=bt_lookup_radiance(owner,complete);
    if(packed.w>6 || !complete || btLookupSurfaces[owner].albedo.w!=1 ||
       !all(isfinite(uv)) || any(uv<0) || any(uv>1))return true;
    int4 key=int4(asint(packed.xyz),int(packed.w)-1);
    int3 u=key.w<2?int3(0,0,1):int3(1,0,0);
    int3 v=key.w<2?int3(0,1,0):key.w<4?int3(0,0,1):int3(0,1,0);
    u*=uv.x<.5?-1:1;v*=uv.y<.5?-1:1;
    float2 weight=abs(uv-.5);
    float3 samples[4];samples[0]=value;
    // Both edge neighbors and the diagonal must certify the same full plane.
    for(uint tap=1;tap<4;++tap) {
        int3 offset=((tap&1)!=0?u:int3(0))+((tap&2)!=0?v:int3(0));
        uint4 neighbor;uint row;
        if(!bt_reconstruction_key(key,offset,neighbor) || !bt_lookup_surface(neighbor,row))return true;
        if(btLookupSurfaces[row].albedo.w!=1)return true;
        bool valid;samples[tap]=bt_lookup_radiance(row,valid);if(!valid)return true;
    }
    value=lerp(lerp(samples[0],samples[1],weight.x),lerp(samples[2],samples[3],weight.x),weight.y);
    return true;
}
#endif
