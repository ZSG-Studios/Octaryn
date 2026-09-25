#ifndef OCTARYN_BLOCK_TRANSPORT_ACTOR_HISTORY
#define OCTARYN_BLOCK_TRANSPORT_ACTOR_HISTORY
#include "Sampling.slang"
#include "PlayerOcclusion.slang"
// The caller supplies the unchanged geometry epoch and the previous resolved link.
bool bt_previous_world_limit(uint4 previous,float3 origin,float3 direction,out float limit) {
    limit=0;
    if(previous.w==1) {
        if(previous.x>=btFrameInfo.z || previous.y!=btFrameInfo.x)return false;
        BtSurface target=btSurfaces[previous.x];
        if(target.state.x!=previous.y || target.extra.x!=previous.z || !bt_valid_key(target.key))return false;
        float3 normal=bt_normal(target.key.w);float denominator=dot(direction,normal);
        if(abs(denominator)<1e-6)return false;
        limit=dot(bt_center(target.key)-origin,normal)/denominator;
        return isfinite(limit) && limit>0 && limit<=raySettings.y;
    }
    if(previous.w==2) {
        limit=bt_sky_distance(origin,direction);
        return isfinite(limit) && limit>0 && limit<=raySettings.y;
    }
    if(previous.w==4) {
        // A blocked link retains the already traced underlying world endpoint.
        limit=asfloat(previous.x);
        return previous.y==btFrameInfo.x && isfinite(limit) && limit>0 && limit<=raySettings.y;
    }
    return false;
}
bool bt_previous_player_changed(BtSurface surface,uint slot,uint4 previous) {
    if(surface.state.w==0 || (previous.w!=1 && previous.w!=2 && previous.w!=4))return false;
    bool blocked=previous.w==4;
    if(playerShadowEnabled==0)return blocked;
    float3 origin,direction;float limit;
    if(!bt_surface_sample(surface,slot,surface.state.y-1,origin,direction) ||
        !bt_inside(origin) || !bt_previous_world_limit(previous,origin,direction,limit))return true;
    if(min(limit,raySettings.y)<=0)return blocked;
    bt_record_player_history_query();
    return bt_player_occluded(origin,direction,limit,0)!=blocked;
}
#endif
