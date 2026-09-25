#include "../Common/OctarynGpuTypes.slang"
#include "VoxelTypes.slang"

static const uint ChunkPaletteEmpty = 1u;
static const uint ChunkPaletteUniform = 2u;
static const uint ChunkPaletteMixed = 4u;

StructuredBuffer<GpuChunkHeader> chunkHeaders : register(t0, space0);
StructuredBuffer<GpuChunkPaletteEntry> chunkPalette : register(t1, space0);
StructuredBuffer<uint> voxelPayloadWords : register(t2, space0);
StructuredBuffer<uint> faceMasks : register(t3, space0);
RWStructuredBuffer<uint> greedyQuadCounts : register(u4, space0);
RWStructuredBuffer<uint> greedyMaterialSums : register(u5, space0);

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

uint material_id_at(GpuChunkHeader header, uint x, uint y, uint z)
{
    if ((header.flags & ChunkPaletteEmpty) != 0u)
    {
        return 0u;
    }
    if ((header.flags & ChunkPaletteUniform) != 0u)
    {
        return chunkPalette[header.paletteOffset].materialId;
    }
    uint index = voxel_index(x, y, z);
    uint paletteIndex = decode_palette_index(header, index);
    return chunkPalette[header.paletteOffset + paletteIndex].materialId;
}

uint3 face_coord(uint direction, uint slice, uint u, uint v)
{
    if (direction < 2u)
    {
        return uint3(slice, u, v);
    }
    if (direction < 4u)
    {
        return uint3(u, slice, v);
    }
    return uint3(u, v, slice);
}

uint face_visible(uint chunkBase, uint direction, uint slice, uint u, uint v)
{
    uint3 coord = face_coord(direction, slice, u, v);
    uint mask = faceMasks[chunkBase + voxel_index(coord.x, coord.y, coord.z)];
    return (mask >> direction) & 1u;
}

uint face_material(GpuChunkHeader header, uint direction, uint slice, uint u, uint v)
{
    uint3 coord = face_coord(direction, slice, u, v);
    return material_id_at(header, coord.x, coord.y, coord.z);
}

uint same_face(GpuChunkHeader header, uint chunkBase, uint direction, uint slice,
               uint u, uint v, uint materialId)
{
    if (face_visible(chunkBase, direction, slice, u, v) == 0u)
    {
        return 0u;
    }
    return face_material(header, direction, slice, u, v) == materialId ? 1u : 0u;
}

uint2 greedy_count_direction(GpuChunkHeader header, uint chunkBase, uint direction)
{
    uint quadCount = 0u;
    uint materialSum = 0u;
    for (uint slice = 0u; slice < OctarynChunkWidthBlocks; ++slice)
    {
        for (uint v = 0u; v < OctarynChunkWidthBlocks; ++v)
        {
            for (uint u = 0u; u < OctarynChunkWidthBlocks; ++u)
            {
                if (face_visible(chunkBase, direction, slice, u, v) == 0u)
                {
                    continue;
                }
                uint materialId = face_material(header, direction, slice, u, v);
                uint continuesU = u > 0u ? same_face(header, chunkBase, direction,
                                                     slice, u - 1u, v,
                                                     materialId) : 0u;
                uint continuesV = v > 0u ? same_face(header, chunkBase, direction,
                                                     slice, u, v - 1u,
                                                     materialId) : 0u;
                if (continuesU != 0u || continuesV != 0u)
                {
                    continue;
                }
                ++quadCount;
                materialSum += materialId;
            }
        }
    }
    return uint2(quadCount, materialSum);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint chunkIndex = dispatchThreadId.x;
    GpuChunkHeader header = chunkHeaders[chunkIndex];
    uint chunkBase = chunkIndex * OctarynChunkWidthBlocks * OctarynChunkWidthBlocks * OctarynChunkWidthBlocks;
    uint quadCount = 0u;
    uint materialSum = 0u;
    for (uint direction = 0u; direction < 6u; ++direction)
    {
        uint2 directionResult = greedy_count_direction(header, chunkBase, direction);
        quadCount += directionResult.x;
        materialSum += directionResult.y;
    }
    greedyQuadCounts[chunkIndex] = quadCount;
    greedyMaterialSums[chunkIndex] = materialSum;
}
