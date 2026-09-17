#ifndef OCTARYN_VOXEL_SURFACE_QUERY
#define OCTARYN_VOXEL_SURFACE_QUERY
#include "VoxelFluidSurface.slang"
#include "../Voxel/WorldGeometry.slang"
#include "../Voxel/WorldVisibility.slang"

struct VoxelSurfaceHit {
    VoxelTraceResult cell;
    float3 normal;
    float2 uv;
    float4 albedo;
    uint flags;
    uint packedVoxel;
    bool backFace;
};
bool voxel_surface_triangle(float3 origin,float3 direction,float3 a,float3 b,float3 c,
                            float minimum,float maximum,out float distance,out float2 barycentric) {
    float3 e1=b-a,e2=c-a,p=cross(direction,e2);
    float determinant=dot(e1,p);distance=0;barycentric=0;
    if(abs(determinant)<1e-8)return false;
    float inverse=1/determinant;float3 offset=origin-a;
    float u=dot(offset,p)*inverse;float3 q=cross(offset,e1);
    float v=dot(direction,q)*inverse;float t=dot(e2,q)*inverse;
    if(u<0 || v<0 || u+v>1 || t<minimum || t>maximum)return false;
    distance=t;barycentric=float2(u,v);return true;
}
bool voxel_surface_visible(VoxelTraceResult candidate,uint direction,out bool known) {
    const int3 offsets[6]={int3(-1,0,0),int3(1,0,0),int3(0,-1,0),int3(0,1,0),int3(0,0,-1),int3(0,0,1)};
    uint neighbor;known=voxel_trace_read_block(candidate.chunk,candidate.voxel+offsets[direction],neighbor);
    if(!known)return false;
    if((blockMaterials[candidate.material].flags&4)!=0)
        return !voxel_same_fluid(candidate.material,neighbor) && (blockMaterials[neighbor].flags&1024)==0;
    return cubeFaceVisible(candidate.material,neighbor);
}

// Surface policy: nearest actual surface, including water/glass. Consumers own
// radiance/transmission policy; the shared query never labels those voxels opaque.
VoxelSurfaceHit voxel_trace_surface(VoxelTraceRay ray,VoxelTraceBounds bounds) {
    VoxelSurfaceHit result=(VoxelSurfaceHit)0;VoxelTraceBuffers world;
    uint remaining=ray.maxSteps;uint4 totalSteps=0;
    if(remaining==0) {result.cell.status=VOXEL_TRACE_INVALID;return result;}
    while(remaining>0) {
        ray.maxSteps=remaining;
        VoxelTraceResult candidate=voxel_trace_candidate(world,ray,bounds);
        uint used=candidate.steps.x+candidate.steps.y+candidate.steps.z+candidate.steps.w;
        totalSteps+=candidate.steps;remaining-=min(remaining,used);
        result.cell=candidate;result.cell.steps=totalSteps;
        if(candidate.status!=VOXEL_TRACE_CANDIDATE)return result;
        if(candidate.material>=voxelTraceMaterialCount) {result.cell.status=VOXEL_TRACE_UNKNOWN;return result;}
        AtlasBlockMaterial material=blockMaterials[candidate.material];result.flags=material.flags;
        if((material.flags&128)!=0) {
            ray.tMin=candidate.exitDistance;
            if(ray.tMin>=ray.tMax) {result.cell.status=VOXEL_TRACE_MISS;return result;}
            continue;
        }
        FluidFace fluid=(FluidFace)0;
        if((material.flags&4)!=0) {
            bool known;fluid=voxel_fluid_face(candidate.material,candidate.chunk,candidate.voxel,known);
            if(!known) {result.cell.status=VOXEL_TRACE_UNKNOWN;return result;}
        }
        float3 blockOrigin=float3((candidate.chunk-ray.originChunk)*32+candidate.voxel);
        float3 origin=ray.origin-blockOrigin;
        // A ray beginning strictly within an opaque cube is already occluded.
        if((material.flags&1)!=0 && (material.flags&(2|4))==0 &&
           candidate.distance==ray.tMin && all(origin+ray.direction*ray.tMin>0) &&
           all(origin+ray.direction*ray.tMin<1)) {
            result.cell.distance=ray.tMin;result.normal=0;result.backFace=true;
            result.albedo=atlasAlbedo.SampleLevel(atlasCutout,float3(.5,.5,material.layers[0]),0);
            return result;
        }
        float nearest=candidate.exitDistance,unknownDistance=voxel_trace_infinity;bool found=false;
        uint faces=(material.flags&2)!=0?((material.flags&32)!=0?6u:4u):6u;
        for(uint faceIndex=0;faceIndex<faces;++faceIndex) {
            uint direction=(material.flags&2)!=0?6+faceIndex:faceIndex;
            uint4 face=uint4(0,0,0,candidate.material|(direction<<16));
            for(uint triangle=0;triangle<2;++triangle) {
                WorldGeometryVertex a=world_geometry_corner(triangle*3,face,fluid);
                WorldGeometryVertex b=world_geometry_corner(triangle*3+1,face,fluid);
                WorldGeometryVertex c=world_geometry_corner(triangle*3+2,face,fluid);
                float distance;float2 barycentric;
                if(!voxel_surface_triangle(origin,ray.direction,a.offset,b.offset,c.offset,
                                           ray.tMin,nearest,distance,barycentric) || distance>=ray.tMax)continue;
                float2 uv=a.uv+(b.uv-a.uv)*barycentric.x+(c.uv-a.uv)*barycentric.y;
                float4 albedo=is_sprite_voxel(a.voxel)?sample_sprite_atlas(atlasAlbedo,uv,a.voxel):
                    atlasAlbedo.SampleLevel(atlasCutout,float3(uv,get_index(a.voxel)),0);
                if((material.flags&1)==0 && albedo.a<.35)continue;
                if((material.flags&2)==0) {
                    bool known;bool visible=voxel_surface_visible(candidate,faceIndex,known);
                    if(!known)unknownDistance=min(unknownDistance,distance);
                    if(!visible)continue;
                }
                float3 normal=normalize(cross(b.offset-a.offset,c.offset-a.offset));
                result.backFace=dot(normal,ray.direction)>0;result.normal=result.backFace?-normal:normal;
                result.uv=uv;result.albedo=albedo;result.packedVoxel=a.voxel;
                result.cell.distance=max(candidate.distance,distance);nearest=distance;found=true;
            }
        }
        if(unknownDistance<=nearest) {result.cell.status=VOXEL_TRACE_UNKNOWN;result.cell.distance=unknownDistance;return result;}
        if(found)return result;
        ray.tMin=candidate.exitDistance;
        if(ray.tMin>=ray.tMax) {result.cell.status=VOXEL_TRACE_MISS;return result;}
    }
    result.cell.status=VOXEL_TRACE_EXHAUSTED;return result;
}
#endif
