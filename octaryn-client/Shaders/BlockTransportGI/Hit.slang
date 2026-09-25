#ifndef OCTARYN_BLOCK_TRANSPORT_HIT
#define OCTARYN_BLOCK_TRANSPORT_HIT
#include "../RayTracing/WorldRayQuery.slang"
#include "Types.slang"
bool bt_hit_key(WorldRayHit hit,out int4 key) {
    key=int4(0,0,0,-1);
    if(!hit.hit || hit.instance==kMapRayInstanceId || hit.surfaceKey.w==0)return false;
    int4 candidate=int4(asint(hit.surfaceKey.xyz),int(hit.surfaceKey.w)-1);
    if(!bt_valid_key(candidate))return false;
    if(world_plant_tag(uint(candidate.w))) {
        uint representative=6+world_plant_plane(uint(candidate.w))*2;
        if(!world_crossed_plant(hit.flags,hit.voxel,representative) ||
            (uint(candidate.w)>>4)!=get_index(hit.voxel))return false;
    } else if(hit.backFace || !world_surface_transport_supported(hit.flags))return false;
    key=candidate;return true;
}
#endif
