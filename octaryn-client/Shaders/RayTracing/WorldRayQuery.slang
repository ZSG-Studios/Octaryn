#ifndef OCTARYN_WORLD_RAY_QUERY_SLANG
#define OCTARYN_WORLD_RAY_QUERY_SLANG
#include "../Materials/MaterialSampling.slang"
#include "../Materials/SpriteSampling.slang"
#include "../Materials/PbrEvaluate.slang"
#include "../Voxel/WorldGeometry.slang"
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
    if(u<0 || v<0 || u+v>1 || t<minimum || t>maximum) return false;
    distance=t;barycentric=float2(u,v);return true;
}
float4 world_ray_sample(Texture2DArray<float4> texture,float2 uv,uint voxel) {
    bool sprite=is_sprite_voxel(voxel);
    if(sprite) {
        float2 texel=.5/atlas_dimensions(texture);
        uv=lerp(texel,1-texel,saturate(uv));
    }
    float3 coordinate=float3(uv,get_index(voxel));
    // Match crisp raster magnification and the same alpha cutoff at mip zero.
    if(raySettings.w<=0) return texture.SampleLevel(atlasCutout,coordinate,0);
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
            result.position=ray.Origin+ray.Direction*result.distance;return result;
        }
    }
    if(raySettings.x==0)return result;
    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(rayScene,anyHit?RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH:RAY_FLAG_NONE,255,ray);
    while(query.Proceed()) {
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
            if(anyHit)return result;
            float3 normal=normalize(cross(b.offset-a.offset,c.offset-a.offset));
            result.backFace=dot(normal,ray.Direction)>0;
            result.normal=result.backFace?-normal:normal;
            result.uv=uv;result.voxel=a.voxel;result.flags=material.flags;
            result.material=face.w&65535;result.instance=instance;result.primitive=primitive;result.albedo=albedo;
        }
    }
    return result;
}
float world_ray_visibility(float3 origin,float3 normal,float3 sunDirection) {
    if(raySettings.x==0 && playerShadowEnabled==0) return 1;
    float3 direction=normalize(sunDirection);
    float3 offset=normal*(dot(normal,direction)>=0?1:-1)*max(raySettings.z,.0001);
    return world_trace_ray(origin+offset,direction,raySettings.y,true).hit?0:1;
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
