#ifndef OCTARYN_WORLD_RAY_QUERY_SLANG
#define OCTARYN_WORLD_RAY_QUERY_SLANG
#include "../Map/MapGeometry.slang"
#include "../Materials/MaterialSampling.slang"
#include "../Materials/SpriteSampling.slang"
#include "../Materials/PbrEvaluate.slang"
#include "../Voxel/WorldGeometry.slang"
// GLB map world geometry; only read for 'MAP'-flagged TLAS instances.
StructuredBuffer<MapVertex> mapVertices;
StructuredBuffer<uint> mapIndices;
StructuredBuffer<MapRayMaterial> mapRayPrimitives;
StructuredBuffer<uint> mapRayTrianglePrimitives;
struct WorldRayRecord {
    ByteAddressBuffer.Handle faces;
    ByteAddressBuffer.Handle fluids;
    uint fluidBase;
    uint faceCount;
    uint2 padding;
};
RaytracingAccelerationStructure rayScene;
RaytracingAccelerationStructure playerShadowScene;
uniform uint playerShadowEnabled;
StructuredBuffer<WorldRayRecord> rayRecords;
uniform float4 raySettings; // enabled, range, origin bias, texture mip
struct WorldRayHit {
    bool hit;
    bool backFace;
    float distance;
    float3 position;
    float3 normal;
    float2 uv;
    uint voxel;
    uint flags;
    uint material;
    uint instance;
    uint primitive;
    float4 albedo;
};
bool world_ray_triangle(float3 origin,float3 direction,float3 a,float3 b,float3 c,
    float minimum,float maximum,out float distance,out float2 barycentric) {
    float3 e1=b-a,e2=c-a,p=cross(direction,e2);
    float determinant=dot(e1,p);
    distance=0;barycentric=0;
    if(abs(determinant)<1e-8) return false;
    float inverse=1/determinant;
    float3 offset=origin-a;
    float u=dot(offset,p)*inverse;
    float3 q=cross(offset,e1);
    float v=dot(direction,q)*inverse;
    float t=dot(e2,q)*inverse;
    // A micron of edge tolerance: shared block edges and quad diagonals are
    // tested per-triangle, and strict rejection let rounding reject a seam hit
    // on both neighbors, opening hairline light cracks between blocks.
    if(u<-1e-6 || v<-1e-6 || u+v>1+1e-6 || t<minimum || t>maximum) return false;
    distance=t;barycentric=float2(u,v);return true;
}
float4 world_ray_sample(Texture2DArray<float4> texture,float2 uv,uint voxel) {
    bool sprite=is_sprite_voxel(voxel);
    if(sprite) {
        float2 texel=.5/atlas_dimensions(texture);
        uv=lerp(texel,1-texel,saturate(uv));
    }
    float3 coordinate=float3(uv,get_index(voxel));
    if(raySettings.w<=0) {
        // Merged cutout quads carry UVs beyond one tile; at every integer block
        // boundary the bilinear sample straddles the atlas wrap seam, where leaf
        // edge texels read near-transparent. The binary shadow alpha test then
        // transmits along those lines, casting thin sun stripes from foliage
        // edges. Snap to the enclosing texel center so the cutout test reads one
        // deterministic texel instead of a cross-seam blend.
        float2 size=atlas_dimensions(texture);
        float2 snapped=(floor(uv*size)+.5)/size;
        return texture.SampleLevel(atlasCutout,float3(snapped,get_index(voxel)),0);
    }
    if(sprite) return texture.SampleLevel(atlasSprite,coordinate,raySettings.w);
    return texture.SampleLevel(atlasLinear,coordinate,raySettings.w);
}
WorldRayHit world_trace_ray(float3 origin,float3 direction,float maxDistance,bool anyHit=false,float3 ignoredBlock=float3(1e30)) {
    WorldRayHit result=(WorldRayHit)0;
    result.distance=min(maxDistance,raySettings.y);
    if((raySettings.x==0 && (!anyHit || playerShadowEnabled==0)) || result.distance<=0) return result;
    RayDesc ray;ray.Origin=origin;ray.Direction=normalize(direction);
    ray.TMin=max(raySettings.z*.25,.0001);ray.TMax=result.distance;
    if(anyHit && playerShadowEnabled!=0) {
        RayQuery<RAY_FLAG_FORCE_OPAQUE|RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> player;
        player.TraceRayInline(playerShadowScene,RAY_FLAG_NONE,255,ray);
        while(player.Proceed()) {}
        if(player.CommittedStatus()==COMMITTED_TRIANGLE_HIT) {
            result.hit=true;result.distance=player.CommittedRayT();
            result.flags=1u<<31;
            result.position=ray.Origin+ray.Direction*result.distance;return result;
        }
    }
    if(raySettings.x==0)return result;
    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(rayScene,anyHit?RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH:RAY_FLAG_NONE,255,ray);
    while(query.Proceed()) {
        // Opaque triangle geometry commits directly; it is never a candidate.
        if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT) {
            if(query.CommittedInstanceID()!=kMapRayInstanceId) continue;
            uint triangle=query.CommittedPrimitiveIndex();
            MapRayMaterial material=mapRayPrimitives[mapRayTrianglePrimitives[triangle]];
            float4 base_color=map_ray_base_color(material);
            // Windows transmit like voxel glass; masked surfaces cut on the
            // authored material factor alpha.
            if(material.alphaMode==2u) continue;
            if(material.alphaMode==1u && base_color.a<material.alphaCutoff) continue;
            uint3 corner=uint3(mapIndices[triangle*3],mapIndices[triangle*3+1],mapIndices[triangle*3+2]);
            float3 a=map_vertex_position(mapVertices[corner.x]);
            float3 b=map_vertex_position(mapVertices[corner.y]);
            float3 c=map_vertex_position(mapVertices[corner.z]);
            float distance;float2 barycentric;
            if(!world_ray_triangle(ray.Origin,ray.Direction,a,b,c,ray.TMin,result.distance,distance,barycentric)) continue;
            float3 geometric=normalize(cross(b-a,c-a));
            bool backFace=dot(geometric,ray.Direction)>0;
            float3 weight=float3(1-barycentric.x-barycentric.y,barycentric.x,barycentric.y);
            float3 normal=map_vertex_normal(mapVertices[corner.x])*weight.x+
                map_vertex_normal(mapVertices[corner.y])*weight.y+
                map_vertex_normal(mapVertices[corner.z])*weight.z;
            normal=dot(normal,normal)>1e-12?normalize(normal):normalize(geometric);
            if(backFace)normal=-normal;
            result.hit=true;result.distance=distance;result.position=ray.Origin+ray.Direction*distance;
            result.normal=normal;result.backFace=backFace;result.uv=float2(0);
            result.albedo=base_color;result.flags=0;result.voxel=0;
            if(anyHit)return result;
            continue;
        }
        if(query.CandidateType()!=CANDIDATE_PROCEDURAL_PRIMITIVE) continue;
        uint instance=query.CandidateInstanceID(),primitive=query.CandidatePrimitiveIndex();
        WorldRayRecord record=rayRecords[instance];
        if(primitive>=record.faceCount) continue;
        uint4 face=record.faces.Load<uint4>(primitive*16);
        AtlasBlockMaterial material=blockMaterials[face.w&65535];
        // Water and glass transmit this single-bounce visibility ray. Lava is solid.
        if((material.flags&4)!=0 && (material.flags&512)==0) continue;
        if((material.flags&16)==0 && (material.flags&2)==0 && (material.flags&512)==0) continue;
        FluidFace fluid;fluid.heights=0;fluid.flow=0;
        if((material.flags&4)!=0) fluid=record.fluids.Load<FluidFace>((primitive-record.fluidBase)*32);
        float3 block=float3(asint(face.xyz));
        if(all(block==ignoredBlock))continue;
        for(uint triangle=0;triangle<2;++triangle) {
            WorldGeometryVertex a=world_geometry_corner(triangle*3,face,fluid);
            WorldGeometryVertex b=world_geometry_corner(triangle*3+1,face,fluid);
            WorldGeometryVertex c=world_geometry_corner(triangle*3+2,face,fluid);
            float distance;float2 barycentric;
            if(!world_ray_triangle(ray.Origin-block,ray.Direction,a.offset,b.offset,c.offset,
                ray.TMin,result.distance,distance,barycentric)) continue;
            float2 uv=a.uv+(b.uv-a.uv)*barycentric.x+(c.uv-a.uv)*barycentric.y;
            float4 albedo=anyHit && (material.flags&1)!=0?float4(1):world_ray_sample(atlasAlbedo,uv,a.voxel);
            if((material.flags&1)==0 && albedo.a<.35) continue;
            query.CommitProceduralPrimitiveHit(distance);
            result.hit=true;result.distance=distance;result.position=ray.Origin+ray.Direction*distance;
            result.albedo=albedo;result.flags=material.flags;result.voxel=a.voxel;
            if(anyHit)return result;
            float3 normal=normalize(cross(b.offset-a.offset,c.offset-a.offset));
            result.backFace=dot(normal,ray.Direction)>0;
            result.normal=result.backFace?-normal:normal;
            result.uv=uv;result.material=face.w&65535;result.instance=instance;result.primitive=primitive;
        }
    }
    return result;
}
float world_ray_visibility(float3 origin,float3 normal,float3 sunDirection) {
    if(raySettings.x==0 && playerShadowEnabled==0) return 1;
    float3 direction=normalize(sunDirection);
    // Same view-stable normal offset as the sun pass. The old sign-flipped bias
    // slid with the camera and flickered specular and DDGI direct lighting.
    float3 offsetOrigin=origin+normal*0.001+direction*0.0005;
    return world_trace_ray(offsetOrigin,direction,raySettings.y,true).hit?0:1;
}
float3 world_ray_emission(WorldRayHit hit) {
    float4 specular=world_ray_sample(atlasSpecular,hit.uv,hit.voxel);
    float emission=specular.a>=1?0:saturate(specular.a*(255.0/254.0));
    float3 catalog=blockMaterials[hit.material].emission.rgb;
    float3 value=hit.albedo.rgb*max(float3(emission),min(catalog,float3(8)));
    if((hit.flags&512)!=0)value=max(value,hit.albedo.rgb+float3(1,.22,.02)*.58);
    return value;
}
float3 world_ray_radiance(WorldRayHit hit,float3 sunDirection,float sunStrength,
    float skyVisibility,float ambientStrength,float skylightFloor) {
    if(!hit.hit) return 0;
    if((hit.flags&512)!=0) return world_ray_emission(hit);
    float ambient=original_ambient_visibility(hit.normal,is_sprite_voxel(hit.voxel),skylightFloor,skyVisibility);
    float direct=max(dot(hit.normal,normalize(sunDirection)),0)*max(sunStrength,0)/3.14159265359;
    if(direct>0) direct*=world_ray_visibility(hit.position,hit.normal,sunDirection);
    return max(hit.albedo.rgb*(original_ambient_light(ambient,skyVisibility)*ambientStrength+direct)+world_ray_emission(hit),0);
}
#endif
