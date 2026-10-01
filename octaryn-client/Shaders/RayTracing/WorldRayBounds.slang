#include "../Materials/MaterialTypes.slang"
StructuredBuffer<AtlasBlockMaterial> blockMaterials;
#include "../World/WorldGeometry.slang"
StructuredBuffer<uint4> rayFaces;
struct RayBounds { float minX; float minY; float minZ; float maxX; float maxY; float maxZ; };
RWStructuredBuffer<RayBounds> rayBounds;
uniform uint rayFaceCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=rayFaceCount) return;
    uint4 face=rayFaces[id.x];
    float3 origin=float3(asint(face.xyz));
    float3 lo=origin+world_geometry_corner(0,face).offset,hi=lo;
    for(uint vertex=1;vertex<6;++vertex) {
        float3 position=origin+world_geometry_corner(vertex,face).offset;
        lo=min(lo,position);hi=max(hi,position);
    }
    // Bounds have thickness; only exact surface intersections commit ray hits.
    float3 padding=max(float3(.001),max(abs(lo),abs(hi))*1e-6);
    lo-=padding;hi+=padding;
    RayBounds box;box.minX=lo.x;box.minY=lo.y;box.minZ=lo.z;
    box.maxX=hi.x;box.maxY=hi.y;box.maxZ=hi.z;
    rayBounds[id.x]=box;
}
