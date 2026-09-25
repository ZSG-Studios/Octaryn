#ifndef OCTARYN_GPU_TYPES_SLANG
#define OCTARYN_GPU_TYPES_SLANG

struct GpuChunkHeader
{
    uint columnIndex;
    uint chunkY;
    uint paletteOffset;
    uint voxelDataOffset;
    uint flags;
    uint uniformBlockId;
    uint neighborMask;
    uint dirtyRevision;
};

struct GpuChunkPaletteEntry
{
    uint blockId;
    uint materialId;
    uint blockFlags;
    uint materialFlags;
};

#endif
