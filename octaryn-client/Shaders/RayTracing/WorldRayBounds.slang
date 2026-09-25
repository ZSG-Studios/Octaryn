#include "../Materials/MaterialTypes.slang"
StructuredBuffer<AtlasBlockMaterial> blockMaterials;
#include "../Voxel/WorldGeometry.slang"
StructuredBuffer<uint4> rayFaces;
StructuredBuffer<FluidFace> rayFluids;
struct RayBounds { float minX; float minY; float minZ; float maxX; float maxY; float maxZ; };
RWStructuredBuffer<RayBounds> rayBounds;
uniform uint rayFaceCount;
uniform uint rayFluidBase;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=rayFaceCount) return;
    uint4 face=rayFaces[id.x];
    FluidFace fluid;fluid.heights=0;fluid.flow=0;
    if((blockMaterials[face.w&65535].flags&4)!=0) fluid=rayFluids[id.x-rayFluidBase];
    float3 origin=float3(asint(face.xyz));
    float3 lo=origin+world_geometry_corner(0,face,fluid).offset,hi=lo;
    for(uint vertex=1;vertex<6;++vertex) {
        float3 position=origin+world_geometry_corner(vertex,face,fluid).offset;
        lo=min(lo,position);hi=max(hi,position);
    }
    // Bounds have thickness; only exact surface intersections commit ray hits.
    float3 padding=max(float3(.001),max(abs(lo),abs(hi))*1e-6);
    lo-=padding;hi+=padding;
    RayBounds box;box.minX=lo.x;box.minY=lo.y;box.minZ=lo.z;
    box.maxX=hi.x;box.maxY=hi.y;box.maxZ=hi.z;
    rayBounds[id.x]=box;
}
