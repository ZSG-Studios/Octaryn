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
    query.TraceRayInline(rayScene,RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,255,ray);
    while(query.Proceed()) {
        if(query.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE) {
            if(query.CandidateInstanceID()!=kMapRayInstanceId)continue;
            MapRayMaterial material=mapRayPrimitives[query.CandidateGeometryIndex()];
            uint triangle=material.padding.x+query.CandidatePrimitiveIndex();
            if(material.alphaMode==0u)return 0;
            MapVertex a=mapVertices[mapIndices[triangle*3]],b=mapVertices[mapIndices[triangle*3+1]],
                c=mapVertices[mapIndices[triangle*3+2]];
            float2 barycentric=query.CandidateTriangleBarycentrics();
            float3 w=float3(1-barycentric.x-barycentric.y,barycentric);
            float2 uv=a.uv*w.x+b.uv*w.y+c.uv*w.z,uv1=a.uv1*w.x+b.uv1*w.y+c.uv1*w.z;
            float alpha=material.baseColor.a*(a.color.a*w.x+b.color.a*w.y+c.color.a*w.z)*
                map_texture_sample(material.textures[0],uv,uv1,float2(0),float2(0),float2(0),float2(0)).a;
            if(material.alphaMode==2u) {
                transmission*=1-saturate(alpha);
                if(transmission<.001)return 0;
                continue;
            }
            if(material.alphaMode==1u && alpha<material.alphaCutoff)continue;
            return 0;
        }
        if(query.CandidateType()!=CANDIDATE_PROCEDURAL_PRIMITIVE)continue;
        WorldRayRecord record=rayRecords[query.CandidateInstanceID()];
        uint primitive=query.CandidatePrimitiveIndex();
        if(primitive>=record.faceCount)continue;
        uint4 face=nonuniform(record.faces).Load<uint4>(primitive*16);
        AtlasBlockMaterial material=blockMaterials[face.w&65535];
        if((material.flags&4)!=0 && (material.flags&512)==0)continue;
        if((material.flags&16)==0 && (material.flags&2)==0 && (material.flags&512)==0)continue;
        // Opposing crossed-sprite faces share one transmissive surface.
        if((material.flags&2)!=0 && (material.flags&32)==0 && ((face.w>>16)&1)!=0)continue;
        FluidFace fluid;fluid.heights=0;fluid.flow=0;
        if((material.flags&4)!=0)fluid=nonuniform(record.fluids).Load<FluidFace>((primitive-record.fluidBase)*32);
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
            bool foliage=(material.flags&256)!=0 || (material.flags&2)!=0;
            if(!foliage)return 0;
            transmission*=lerp(.35,.1,saturate(albedo.a));
            if(transmission<.001)return 0;
            break;
        }
    }
    return query.CommittedStatus()==COMMITTED_TRIANGLE_HIT?0:transmission;
}
#endif
