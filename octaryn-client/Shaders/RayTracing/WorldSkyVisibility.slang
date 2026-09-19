#ifndef OCTARYN_WORLD_SKY_VISIBILITY
#define OCTARYN_WORLD_SKY_VISIBILITY
#include "WorldRayQuery.slang"

// Transmission is order independent. Never stop at foliage before checking
// whether opaque geometry closes the same sky direction farther away.
float world_sky_transmission(float3 origin,float3 direction,float maxDistance) {
    RayDesc ray;ray.Origin=origin;ray.Direction=normalize(direction);
    ray.TMin=max(raySettings.z*.25,.0001);ray.TMax=min(maxDistance,raySettings.y);
    if(ray.TMax<=ray.TMin)return 0;
    if(playerShadowEnabled!=0) {
        RayQuery<RAY_FLAG_FORCE_OPAQUE|RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> player;
        player.TraceRayInline(playerShadowScene,RAY_FLAG_NONE,255,ray);
        while(player.Proceed()) {}
        if(player.CommittedStatus()==COMMITTED_TRIANGLE_HIT)return 0;
    }
    if(raySettings.x==0)return 1;
    float transmission=1;
    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(rayScene,RAY_FLAG_NONE,255,ray);
    while(query.Proceed()) {
        // Map triangles commit directly; glass attenuates, leaves-like masks
        // cut, opaque architecture closes the sky direction.
        if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT) {
            if(query.CommittedInstanceID()!=kMapRayInstanceId)continue;
            uint triangle=query.CommittedPrimitiveIndex();
            MapRayMaterial material=mapRayPrimitives[mapRayTrianglePrimitives[triangle]];
            float4 base_color=map_ray_base_color(material);
            if(material.alphaMode==2u) {
                transmission*=lerp(.35,.1,saturate(base_color.a));
                if(transmission<.001)return 0;
                continue;
            }
            if(material.alphaMode==1u && base_color.a<material.alphaCutoff)continue;
            return 0;
        }
        if(query.CandidateType()!=CANDIDATE_PROCEDURAL_PRIMITIVE)continue;
        WorldRayRecord record=rayRecords[query.CandidateInstanceID()];
        uint primitive=query.CandidatePrimitiveIndex();
        if(primitive>=record.faceCount)continue;
        uint4 face=record.faces.Load<uint4>(primitive*16);
        AtlasBlockMaterial material=blockMaterials[face.w&65535];
        if((material.flags&4)!=0 && (material.flags&512)==0)continue;
        if((material.flags&16)==0 && (material.flags&2)==0 && (material.flags&512)==0)continue;
        FluidFace fluid;fluid.heights=0;fluid.flow=0;
        if((material.flags&4)!=0)fluid=record.fluids.Load<FluidFace>((primitive-record.fluidBase)*32);
        float3 block=float3(asint(face.xyz));
        for(uint triangle=0;triangle<2;++triangle) {
            WorldGeometryVertex a=world_geometry_corner(triangle*3,face,fluid);
            WorldGeometryVertex b=world_geometry_corner(triangle*3+1,face,fluid);
            WorldGeometryVertex c=world_geometry_corner(triangle*3+2,face,fluid);
            float distance;float2 barycentric;
            if(!world_ray_triangle(ray.Origin-block,ray.Direction,a.offset,b.offset,c.offset,
                ray.TMin,ray.TMax,distance,barycentric))continue;
            float2 uv=a.uv+(b.uv-a.uv)*barycentric.x+(c.uv-a.uv)*barycentric.y;
            float4 albedo=(material.flags&1)!=0?float4(1):world_ray_sample(atlasAlbedo,uv,a.voxel);
            if((material.flags&1)==0 && albedo.a<.35)continue;
            bool foliage=(material.flags&256)!=0 || ((material.flags&2)!=0 && (material.flags&16)==0);
            if(!foliage)return 0;
            transmission*=lerp(.35,.1,saturate(albedo.a));
            if(transmission<.001)return 0;
            break;
        }
    }
    return transmission;
}
#endif
