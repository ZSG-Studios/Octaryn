#include "VoxelTypes.slang"

uint voxel_quad_local_x(PackedVoxelQuad16 quad)
{
    return quad.pos_size_dir_lod & 31u;
}

uint voxel_quad_face_dir(PackedVoxelQuad16 quad)
{
    return (quad.pos_size_dir_lod >> 25u) & 7u;
}
