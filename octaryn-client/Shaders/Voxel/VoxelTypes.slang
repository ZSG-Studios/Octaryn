#ifndef OCTARYN_VOXEL_TYPES_SLANG
#define OCTARYN_VOXEL_TYPES_SLANG

static const uint OctarynChunkWidthBlocks = 32;
static const uint OctarynColumnHeightChunks = 32;
static const uint OctarynWorldHeightBlocks = 1024;

struct PackedVoxelQuad16
{
    uint pos_size_dir_lod;
    uint material_flags;
    uint light_ao_uv;
    uint chunk_batch_meta;
};

#endif
