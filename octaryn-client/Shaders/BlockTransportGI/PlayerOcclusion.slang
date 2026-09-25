#ifndef OCTARYN_BLOCK_TRANSPORT_PLAYER_OCCLUSION
#define OCTARYN_BLOCK_TRANSPORT_PLAYER_OCCLUSION
bool bt_player_occluded(float3 origin,float3 direction,float distance,float minimumDistance=-1) {
    if(playerShadowEnabled==0 || distance<=0)return false;
    RayDesc ray;ray.Origin=origin;ray.Direction=direction;
    ray.TMin=minimumDistance>=0?minimumDistance:max(raySettings.z*.25,.0001);ray.TMax=min(distance,raySettings.y);
    if(ray.TMax<=ray.TMin)return false;
    RayQuery<RAY_FLAG_FORCE_OPAQUE|RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(playerShadowScene,RAY_FLAG_NONE,255,ray);
    while(query.Proceed()) {}
    return query.CommittedStatus()==COMMITTED_TRIANGLE_HIT;
}
#endif
