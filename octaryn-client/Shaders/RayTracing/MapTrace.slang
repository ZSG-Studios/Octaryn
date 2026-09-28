#ifndef OCTARYN_MAP_TRACE
#define OCTARYN_MAP_TRACE
#include "WorldRayQuery.slang"
// Map-only traversal returns geometry identity; material shading happens later.
WorldRayHit map_trace_intersection(float3 origin,float3 direction,float maximum,bool visibility=false) {
    WorldRayHit hit=(WorldRayHit)0;
    hit.distance=min(maximum,raySettings.y);
    if(raySettings.x==0 || hit.distance<=.0005)return hit;
    RayDesc ray;ray.Origin=origin;ray.Direction=direction;ray.TMin=.0005;ray.TMax=hit.distance;
    {
        RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
        query.TraceRayInline(rayScene,visibility?RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH:RAY_FLAG_NONE,255,ray);
        while(query.Proceed()) {
            if(query.CandidateType()!=CANDIDATE_NON_OPAQUE_TRIANGLE)continue;
            uint instance=query.CandidateInstanceID(),geometry=query.CandidateGeometryIndex();
            if(!is_map_ray_instance(instance))continue;
            MapRayMaterial material=map_ray_material(instance,geometry);
            if(material.alphaMode==2)continue;
            if(material.alphaMode==1) {
                uint triangle=material.padding.x+query.CandidatePrimitiveIndex();
                MapVertex a=map_ray_vertex(instance,triangle*3),b=map_ray_vertex(instance,triangle*3+1),
                    c=map_ray_vertex(instance,triangle*3+2);
                float2 barycentric=query.CandidateTriangleBarycentrics();
                float3 weights=float3(1-barycentric.x-barycentric.y,barycentric);
                float alpha=material.baseColor.a*dot(float3(a.color.a,b.color.a,c.color.a),weights);
                if(material.textures[0].present!=0) {
                    float2 uv=a.uv*weights.x+b.uv*weights.y+c.uv*weights.z;
                    float2 uv1=a.uv1*weights.x+b.uv1*weights.y+c.uv1*weights.z;
                    alpha*=nonuniform(material.textures[0].image).SampleLevel(nonuniform(material.textures[0].sampler),
                        map_texture_uv(material.textures[0],uv,uv1),0).a;
                }
                if(alpha<material.alphaCutoff)continue;
            }
            query.CommitNonOpaqueTriangleHit();
        }
        if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT) {
            hit.hit=true;hit.instance=query.CommittedInstanceID();hit.material=query.CommittedGeometryIndex();
            hit.primitive=query.CommittedPrimitiveIndex();hit.distance=query.CommittedRayT();
            hit.uv=query.CommittedTriangleBarycentrics();
        }
    }
    if(visibility)return hit;
    if(hit.hit) {
        hit.position=origin+direction*hit.distance;
        hit.primitive+=map_ray_material(hit.instance,hit.material).padding.x;
    }
    return hit;
}
#endif
