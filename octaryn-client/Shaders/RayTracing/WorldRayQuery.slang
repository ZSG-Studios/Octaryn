#ifndef OCTARYN_WORLD_RAY_QUERY_SLANG
#define OCTARYN_WORLD_RAY_QUERY_SLANG
#include "MapRayGeometry.slang"
#include "../Materials/MaterialSampling.slang"
#include "../Materials/SpriteSampling.slang"
#include "../Materials/PbrEvaluate.slang"
#include "../World/WorldGeometry.slang"
#include "../World/WorldSurfaceKey.slang"
struct WorldRayRecord {
    ByteAddressBuffer.Handle faces;
    uint faceCount;
    uint2 padding;
};
RaytracingAccelerationStructure rayScene;
// Dormant contract: the avatar shadow scene slot stays bound to the world
// TLAS with a zero flag, disabling every player-branch on this platform.
RaytracingAccelerationStructure playerShadowScene;
uniform uint playerShadowEnabled;
StructuredBuffer<WorldRayRecord> rayRecords;
uniform float4 raySettings; // enabled, range, origin bias, texture mip
uniform uint mapDynamicInstanceStart;
struct WorldRayHit {
    bool hit;
    bool backFace;
    bool unlit;
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
    float metallic;
    float roughness;
    float3 emission;
    uint4 surfaceKey;
};
bool world_ray_dynamic(WorldRayHit hit) {
    return hit.hit && map_ray_dynamic_instance(hit.instance,mapDynamicInstanceStart);
}
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
    // Treat shared voxel edges as closed. Independent triangles of adjacent
    // merged faces can differ by a few ulps at grazing sun angles; a symmetric
    // tolerance prevents both triangles from rejecting the same blocker.
    if(u<-1e-5 || v<-1e-5 || u+v>1+1e-5 || t<minimum || t>maximum) return false;
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
bool world_face_contains_block(uint4 face,float3 ignoredBlock) {
    uint encodedDirection=(face.w>>16)&15;
    if(encodedDirection>=6)return false;
    const uint catalogDirections[6]={3,2,5,4,1,0};
    uint direction=catalogDirections[encodedDirection];
    float2 extent=float2(((face.w>>20)&31)+1,((face.w>>25)&31)+1);
    float3 relative=ignoredBlock-float3(asint(face.xyz));
    bool u=relative.x>=0 && relative.x<extent.x;
    bool v=relative.y>=0 && relative.y<extent.y;
    bool w=relative.z>=0 && relative.z<extent.x;
    if(direction<2)return u&&v&&relative.z==0;
    if(direction<4)return relative.x==0&&v&&w;
    return u&&relative.y==0&&relative.z>=0&&relative.z<extent.y;
}
WorldRayHit world_map_ray_surface(WorldRayHit hit,float3 direction,bool geometryOnly=false) {
    uint triangle=hit.primitive;
    MapVertex va=map_ray_vertex(hit.instance,triangle*3),vb=map_ray_vertex(hit.instance,triangle*3+1),
        vc=map_ray_vertex(hit.instance,triangle*3+2);
    float3 a=map_vertex_position(va),b=map_vertex_position(vb),c=map_vertex_position(vc);
    float3 geometric=normalize(cross(b-a,c-a))*map_ray_orientation(hit.instance);
    bool backFace=dot(geometric,direction)>0;
    // Geometry-only queries consume distance and geometric sidedness only.
    hit.backFace=backFace;
    if(geometryOnly)return hit;
    MapRayMaterial material=map_ray_material(hit.instance,hit.material);
    float3 weight=float3(1-hit.uv.x-hit.uv.y,hit.uv.x,hit.uv.y);
    float3 normal=map_vertex_normal(va)*weight.x+map_vertex_normal(vb)*weight.y+map_vertex_normal(vc)*weight.z;
    normal=dot(normal,normal)>1e-12?normalize(normal):geometric;
    float2 uv=va.uv*weight.x+vb.uv*weight.y+vc.uv*weight.z;
    float2 uv1=va.uv1*weight.x+vb.uv1*weight.y+vc.uv1*weight.z;
    float4 tangent=va.tangent*weight.x+vb.tangent*weight.y+vc.tangent*weight.z;
    if(material.textures[2].present!=0 &&
        (dot(tangent.xyz,tangent.xyz)<1e-10 || map_texture_frame_changed(material.textures[2]))) {
        float2 ta=map_texture_uv(material.textures[2],va.uv,va.uv1);
        float2 tb=map_texture_uv(material.textures[2],vb.uv,vb.uv1)-ta;
        float2 tc=map_texture_uv(material.textures[2],vc.uv,vc.uv1)-ta;
        float determinant=tb.x*tc.y-tb.y*tc.x;
        tangent.xyz=abs(determinant)>1e-10?((b-a)*tc.y-(c-a)*tb.y)/determinant:float3(0);
        float3 bitangent=abs(determinant)>1e-10?((c-a)*tb.x-(b-a)*tc.x)/determinant:float3(0);
        tangent.w=dot(cross(normal,tangent.xyz),bitangent)<0?-1:1;
    }
    MapSurface surface=map_surface(material,uv,uv1,va.color*weight.x+vb.color*weight.y+vc.color*weight.z,
        normal,tangent.xyz,tangent.w,va.blend0*weight.x+vb.blend0*weight.y+vc.blend0*weight.z,
        va.blend1*weight.x+vb.blend1*weight.y+vc.blend1*weight.z,float2(0),float2(0),float2(0),float2(0));
    hit.normal=backFace?-surface.normal:surface.normal;hit.backFace=backFace;
    hit.uv=uv;hit.albedo=surface.albedo;hit.metallic=surface.metallic;
    hit.roughness=surface.roughness;hit.emission=surface.emission;hit.unlit=map_material_unlit(material);return hit;
}
WorldRayHit world_trace_ray(float3 origin,float3 direction,float maxDistance,bool anyHit=false,float3 ignoredBlock=float3(1e30),bool geometryOnly=false,bool includePlayer=true,bool classifyTransmission=false,float minimumDistance=-1) {
    WorldRayHit result=(WorldRayHit)0;
    result.distance=min(maxDistance,raySettings.y);
    if((raySettings.x==0 && (!anyHit || playerShadowEnabled==0)) || result.distance<=0) return result;
    float minimum=minimumDistance>=0?minimumDistance:max(raySettings.z*.25,.0001);
    if(!isfinite(result.distance) || !isfinite(minimum) || result.distance<=minimum ||
        !all(isfinite(origin)) || !all(isfinite(direction)) || dot(direction,direction)<=1e-20)return result;
    RayDesc ray;ray.Origin=origin;ray.Direction=normalize(direction);
    ray.TMin=minimum;ray.TMax=result.distance;
    if(anyHit && includePlayer && playerShadowEnabled!=0) {
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
    // Keep the traversal object in a short lexical scope.  The query carries
    // backend traversal state and can consume many live registers; shading the
    // selected hit must not extend that lifetime into texture/BRDF evaluation.
    bool committedTriangle=false;
    uint committedInstance=0,committedGeometry=0,committedPrimitive=0;
    float committedDistance=0;
    float2 committedBarycentrics=0;
    {
#ifdef OCTARYN_MAP_ONLY_RAYS
      RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
#else
      RayQuery<RAY_FLAG_NONE> query;
#endif
      query.TraceRayInline(rayScene,anyHit?RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH:RAY_FLAG_NONE,255,ray);
      while(query.Proceed()) {
        if(query.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE) {
            if(!is_map_ray_instance(query.CandidateInstanceID())) continue;
            uint2 identity=map_ray_identity(query.CandidateInstanceID(),query.CandidateGeometryIndex(),query.CandidatePrimitiveIndex());
            uint materialIndex=identity.x;
            MapRayMaterial material=map_ray_material(query.CandidateInstanceID(),materialIndex);
            uint triangle=identity.y;
            // Blended transmission is outside this opaque single-bounce tracer.
            if(material.alphaMode==2u && !classifyTransmission) continue;
            float distance=query.CandidateTriangleRayT();
            if(distance>result.distance)continue;
            float2 barycentric=query.CandidateTriangleBarycentrics();
            // Visibility needs only authored mask alpha. Opaque geometry reads
            // neither vertex attributes nor textures while traversing the BVH.
            if(material.alphaMode==1u) {
                MapVertex va=map_ray_vertex(query.CandidateInstanceID(),triangle*3),vb=map_ray_vertex(query.CandidateInstanceID(),triangle*3+1),
                    vc=map_ray_vertex(query.CandidateInstanceID(),triangle*3+2);
                float3 weight=float3(1-barycentric.x-barycentric.y,barycentric.x,barycentric.y);
                float2 uv=va.uv*weight.x+vb.uv*weight.y+vc.uv*weight.z;
                float2 uv1=va.uv1*weight.x+vb.uv1*weight.y+vc.uv1*weight.z;
                float alpha=material.baseColor.a*(va.color.a*weight.x+vb.color.a*weight.y+vc.color.a*weight.z);
                if(material.textures[0].present!=0)
                    alpha*=nonuniform(material.textures[0].image).SampleLevel(nonuniform(material.textures[0].sampler),
                        map_texture_uv(material.textures[0],uv,uv1),0).a;
                if(alpha<material.alphaCutoff)continue;
            }
            query.CommitNonOpaqueTriangleHit();
            result.hit=true;result.distance=distance;result.position=ray.Origin+ray.Direction*distance;
            result.uv=barycentric;result.flags=0;result.voxel=6;
            result.instance=query.CandidateInstanceID();result.primitive=triangle;result.material=materialIndex;
            if(anyHit)return result;
            continue;
        }
#ifndef OCTARYN_MAP_ONLY_RAYS
        if(query.CandidateType()!=CANDIDATE_PROCEDURAL_PRIMITIVE) continue;
        uint instance=query.CandidateInstanceID(),primitive=query.CandidatePrimitiveIndex();
        WorldRayRecord record=rayRecords[instance];
        if(primitive>=record.faceCount) continue;
        // Each ray can visit a different column in the same wave. Descriptor
        // handles default to wave-uniform; that would read a neighbor's mesh.
        uint4 face=nonuniform(record.faces).Load<uint4>(primitive*16);
        AtlasBlockMaterial material=blockMaterials[face.w&65535];
        // Water and glass transmit this single-bounce visibility ray. Lava is solid.
        bool transmission=((material.flags&4)!=0 && (material.flags&512)==0) ||
            ((material.flags&16)==0 && (material.flags&2)==0 && (material.flags&512)==0);
        if(transmission && !classifyTransmission)continue;
        float3 block=float3(asint(face.xyz));
        if(world_face_contains_block(face,ignoredBlock))continue;
        for(uint triangle=0;triangle<2;++triangle) {
            WorldGeometryVertex a=world_geometry_corner(triangle*3,face);
            WorldGeometryVertex b=world_geometry_corner(triangle*3+1,face);
            WorldGeometryVertex c=world_geometry_corner(triangle*3+2,face);
            float distance;float2 barycentric;
            if(!world_ray_triangle(ray.Origin-block,ray.Direction,a.offset,b.offset,c.offset,
                ray.TMin,result.distance,distance,barycentric)) continue;
            float2 uv=a.uv+(b.uv-a.uv)*barycentric.x+(c.uv-a.uv)*barycentric.y;
            float4 albedo=anyHit && (material.flags&1)!=0?float4(1):world_ray_sample(atlasAlbedo,uv,a.voxel);
            if((material.flags&1)==0 && albedo.a<.35 && !(transmission && classifyTransmission)) continue;
            query.CommitProceduralPrimitiveHit(distance);
            result.hit=true;result.distance=distance;result.position=ray.Origin+ray.Direction*distance;
            result.albedo=albedo;result.flags=material.flags|(transmission?(1u<<30):0);result.voxel=a.voxel;
            if(anyHit)return result;
            result.surfaceKey=0;
            if((material.flags&4)==0) {
                float3 localHit=a.offset+(b.offset-a.offset)*barycentric.x+(c.offset-a.offset)*barycentric.y;
                result.surfaceKey=world_crossed_plant(material.flags,a.voxel,(face.w>>16)&15)?
                    world_plant_surface_key(face,a.voxel,-ray.Direction):world_surface_key(face,localHit);
            }
            float3 normal=normalize(cross(b.offset-a.offset,c.offset-a.offset));
            result.backFace=dot(normal,ray.Direction)>0;
            result.normal=result.backFace?-normal:normal;
            result.uv=uv;result.material=face.w&65535;result.instance=instance;result.primitive=primitive;
        }
#endif
      }
      // Opaque map triangles are accepted in hardware, without candidate
      // callbacks. Copy the committed payload before the query leaves scope.
      committedTriangle=query.CommittedStatus()==COMMITTED_TRIANGLE_HIT;
      if(committedTriangle) {
        committedDistance=query.CommittedRayT();
        committedInstance=query.CommittedInstanceID();
        committedGeometry=query.CommittedGeometryIndex();
        committedPrimitive=query.CommittedPrimitiveIndex();
        committedBarycentrics=query.CommittedTriangleBarycentrics();
      }
    }
    // Opaque map triangles are committed by hardware without entering the
    // candidate callback. Publish that committed hit before the any-hit early
    // return; otherwise visibility rays treat imported map walls as misses and
    // let direct sun pass through them.
    if(committedTriangle && is_map_ray_instance(committedInstance)) {
        result.hit=true;result.distance=committedDistance;
        result.position=ray.Origin+ray.Direction*committedDistance;
        result.instance=committedInstance;
        uint2 identity=map_ray_identity(committedInstance,committedGeometry,committedPrimitive);
        result.material=identity.x;result.primitive=identity.y;
        result.uv=committedBarycentrics;result.flags=0;result.voxel=6;
    }
    if(anyHit)return result;
#ifdef OCTARYN_MAP_DEFERRED_MATERIAL
    // The fused direct-map reflection variant resolves the selected surface
    // after visibility. Keep barycentrics until that material evaluation.
    if(!geometryOnly)return result;
#endif
    // Shade only the final nearest map surface, never every accepted candidate.
    if(result.hit && is_map_ray_instance(result.instance))return world_map_ray_surface(result,ray.Direction,geometryOnly);
    return result;
}
float world_ray_visibility(float3 origin,float3 normal,float3 sunDirection) {
    if(raySettings.x==0 && playerShadowEnabled==0) return 1;
    float3 direction=normalize(sunDirection);
    // Same view-stable normal offset as the sun pass. The old sign-flipped bias
    // slid with the camera and flickered specular and direct lighting.
    float3 offsetOrigin=origin+normal*0.001+direction*0.0005;
    return world_trace_ray(offsetOrigin,direction,raySettings.y,true).hit?0:1;
}
float3 world_ray_emission(WorldRayHit hit) {
#ifdef OCTARYN_MAP_ONLY_RAYS
    return hit.emission;
#else
    if(is_map_ray_instance(hit.instance))return hit.emission;
    float4 specular=world_ray_sample(atlasSpecular,hit.uv,hit.voxel);
    float emission=specular.a>=1?0:saturate(specular.a*(255.0/254.0));
    float3 catalog=blockMaterials[hit.material].emission.rgb;
    float3 value=hit.albedo.rgb*max(float3(emission),min(catalog,float3(8)));
    if((hit.flags&512)!=0)value=max(value,hit.albedo.rgb+float3(1,.22,.02)*.58);
    return value;
#endif
}
float world_ray_metallic(WorldRayHit hit) {
#ifdef OCTARYN_MAP_ONLY_RAYS
    return hit.metallic;
#else
    if(is_map_ray_instance(hit.instance))return hit.metallic;
    return world_ray_sample(atlasSpecular,hit.uv,hit.voxel).g>=(230.0/255.0)?1:0;
#endif
}
float world_ray_roughness(WorldRayHit hit) {
#ifdef OCTARYN_MAP_ONLY_RAYS
    return hit.roughness;
#else
    if(is_map_ray_instance(hit.instance))return hit.roughness;
    return clamp(pow(1-saturate(world_ray_sample(atlasSpecular,hit.uv,hit.voxel).r),2),.02,1);
#endif
}
float3 world_ray_radiance(WorldRayHit hit,float3 sunDirection,float sunStrength,
    float skyVisibility,float ambientStrength,float skylightFloor) {
    if(!hit.hit) return 0;
    if(hit.unlit)return hit.albedo.rgb;
    if((hit.flags&512)!=0) return world_ray_emission(hit);
    float ambient=original_ambient_visibility(hit.normal,is_sprite_voxel(hit.voxel),skylightFloor,skyVisibility);
    float direct=max(dot(hit.normal,normalize(sunDirection)),0)*max(sunStrength,0)/3.14159265359;
    if(direct>0) direct*=world_ray_visibility(hit.position,hit.normal,sunDirection);
    return max(hit.albedo.rgb*(original_ambient_light(ambient,skyVisibility)*ambientStrength+direct)+world_ray_emission(hit),0);
}
#endif
