#include "WorldRaster.slang"
struct WorldBatchRecord {
    ByteAddressBuffer.Handle faceBuffer;
    ByteAddressBuffer.Handle patchBuffer;
    uint patchBase;
    uint padding;
};
StructuredBuffer<WorldBatchRecord> batchRecords;
[shader("vertex")]
SurfaceVertexOutput batch_vertex_main(uint vertex:SV_VertexID,
    uint localInstance:SV_InstanceID,uint recordIndex:SV_StartInstanceLocation) {
    WorldBatchRecord record=batchRecords[recordIndex];
    uint patch=record.patchBuffer.Load<uint>((record.patchBase+localInstance)*4);
    uint4 face=record.faceBuffer.Load<uint4>((patch>>6)*16);
    uint2 extent=uint2(((face.w>>20)&31)+1,((face.w>>25)&31)+1);
    FluidFace fluid;fluid.heights=0;fluid.flow=0;
    return world_surface_output(world_vertex_coordinate_with_fluid(vertex,face,world_patch_corner(extent,patch&63,vertex),fluid),face.w&65535);
}
