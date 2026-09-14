#include "../Common/OctarynGpuTypes.slang"
#include "VoxelTypes.slang"

static const uint ChunkPaletteEmpty = 1u;
static const uint ChunkPaletteUniform = 2u;
static const uint ChunkPaletteMixed = 4u;

StructuredBuffer<GpuChunkHeader> chunkHeaders : register(t0, space0);
StructuredBuffer<GpuChunkPaletteEntry> chunkPalette : register(t1, space0);
StructuredBuffer<uint> voxelPayloadWords : register(t2, space0);
RWStructuredBuffer<uint> faceMasks : register(u3, space0);
RWStructuredBuffer<uint> faceCounts : register(u4, space0);

uint voxel_index(uint x, uint y, uint z)
{
    return x + OctarynChunkWidthBlocks * (y + OctarynChunkWidthBlocks * z);
}

uint load_payload_byte(uint byteOffset)
{
    uint word = voxelPayloadWords[byteOffset >> 2u];
    return (word >> ((byteOffset & 3u) * 8u)) & 0xffu;
}

uint decode_palette_index(GpuChunkHeader header, uint voxelIndex)
{
    uint bitOffset = voxelIndex * 2u;
    uint packedByte = load_payload_byte(header.voxelDataOffset + (bitOffset >> 3u));
    return (packedByte >> (bitOffset & 7u)) & 3u;
}

uint block_id_at(GpuChunkHeader header, int x, int y, int z)
{
    if (x < 0 || y < 0 || z < 0 ||
        x >= int(OctarynChunkWidthBlocks) ||
        y >= int(OctarynChunkWidthBlocks) ||
        z >= int(OctarynChunkWidthBlocks))
    {
        return 0u;
    }
    if ((header.flags & ChunkPaletteEmpty) != 0u)
    {
        return 0u;
    }
    if ((header.flags & ChunkPaletteUniform) != 0u)
    {
        return header.uniformBlockId;
    }
    uint index = voxel_index(uint(x), uint(y), uint(z));
    uint paletteIndex = decode_palette_index(header, index);
    return chunkPalette[header.paletteOffset + paletteIndex].blockId;
}

uint visible_mask(GpuChunkHeader header, uint x, uint y, uint z)
{
    int sx = int(x);
    int sy = int(y);
    int sz = int(z);
    uint currentSolid = block_id_at(header, sx, sy, sz) == 0u ? 0u : 1u;
    if (currentSolid == 0u)
    {
        return 0u;
    }
    uint mask = 0u;
    mask |= block_id_at(header, sx + 1, sy, sz) == 0u ? 1u : 0u;
    mask |= block_id_at(header, sx - 1, sy, sz) == 0u ? 2u : 0u;
    mask |= block_id_at(header, sx, sy + 1, sz) == 0u ? 4u : 0u;
    mask |= block_id_at(header, sx, sy - 1, sz) == 0u ? 8u : 0u;
    mask |= block_id_at(header, sx, sy, sz + 1) == 0u ? 16u : 0u;
    mask |= block_id_at(header, sx, sy, sz - 1) == 0u ? 32u : 0u;
    return mask;
}

uint mask_face_count(uint mask)
{
    return (mask & 1u) + ((mask >> 1u) & 1u) + ((mask >> 2u) & 1u) +
           ((mask >> 3u) & 1u) + ((mask >> 4u) & 1u) + ((mask >> 5u) & 1u);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint chunkIndex = dispatchThreadId.x;
    GpuChunkHeader header = chunkHeaders[chunkIndex];
    uint baseIndex = chunkIndex * OctarynChunkWidthBlocks * OctarynChunkWidthBlocks * OctarynChunkWidthBlocks;
    uint faces = 0u;
    for (uint z = 0u; z < OctarynChunkWidthBlocks; ++z)
    {
        for (uint y = 0u; y < OctarynChunkWidthBlocks; ++y)
        {
            for (uint x = 0u; x < OctarynChunkWidthBlocks; ++x)
            {
                uint index = voxel_index(x, y, z);
                uint mask = visible_mask(header, x, y, z);
                faceMasks[baseIndex + index] = mask;
                faces += mask_face_count(mask);
            }
        }
    }
    faceCounts[chunkIndex] = faces;
}
