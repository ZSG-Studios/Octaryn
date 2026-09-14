#include "../Common/OctarynGpuTypes.slang"
#include "VoxelTypes.slang"

static const uint ChunkPaletteEmpty = 1u;
static const uint ChunkPaletteUniform = 2u;
static const uint ChunkPaletteMixed = 4u;

StructuredBuffer<GpuChunkHeader> chunkHeaders : register(t0, space0);
StructuredBuffer<GpuChunkPaletteEntry> chunkPalette : register(t1, space0);
StructuredBuffer<uint> voxelPayloadWords : register(t2, space0);
RWStructuredBuffer<uint> occupancyCounts : register(u3, space0);

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

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint chunkIndex = dispatchThreadId.x;
    GpuChunkHeader header = chunkHeaders[chunkIndex];

    if ((header.flags & ChunkPaletteEmpty) != 0u)
    {
        occupancyCounts[chunkIndex] = 0u;
        return;
    }

    if ((header.flags & ChunkPaletteUniform) != 0u)
    {
        occupancyCounts[chunkIndex] =
            header.uniformBlockId == 0u ? 0u : OctarynChunkWidthBlocks * OctarynChunkWidthBlocks * OctarynChunkWidthBlocks;
        return;
    }

    uint occupied = 0u;
    for (uint voxelIndex = 0u; voxelIndex < OctarynChunkWidthBlocks * OctarynChunkWidthBlocks * OctarynChunkWidthBlocks; ++voxelIndex)
    {
        uint paletteIndex = decode_palette_index(header, voxelIndex);
        uint blockId = chunkPalette[header.paletteOffset + paletteIndex].blockId;
        occupied += blockId == 0u ? 0u : 1u;
    }
    occupancyCounts[chunkIndex] = occupied;
}
