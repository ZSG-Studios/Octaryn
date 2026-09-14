#include "../Common/OctarynGpuTypes.slang"
#include "VoxelTypes.slang"

static const uint ChunkPaletteEmpty = 1u;
static const uint ChunkPaletteUniform = 2u;
static const uint ProbeChunkVoxelCount = 32768u;

StructuredBuffer<GpuChunkHeader> chunkHeaders : register(t0, space0);
StructuredBuffer<GpuChunkPaletteEntry> chunkPalette : register(t1, space0);
StructuredBuffer<uint> voxelPayloadWords : register(t2, space0);
StructuredBuffer<uint> faceMasks : register(t3, space0);
StructuredBuffer<uint> greedyQuadCounts : register(t4, space0);
StructuredBuffer<uint> greedyQuadOffsets : register(t5, space0);
RWStructuredBuffer<PackedVoxelQuad16> packedQuads : register(u6, space0);
RWStructuredBuffer<uint> emittedQuadCounts : register(u7, space0);
RWStructuredBuffer<uint> emittedMaterialSums : register(u8, space0);
RWStructuredBuffer<uint> emittedChecksums : register(u9, space0);

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
    uint paletteIndex = decode_palette_index(header, voxel_index(x, y, z));
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

uint pack_pos_size_dir_lod(uint3 coord, uint direction)
{
    return (coord.x & 31u) |
           ((coord.y & 31u) << 5u) |
           ((coord.z & 31u) << 10u) |
           (direction << 25u);
}

PackedVoxelQuad16 pack_probe_quad(uint3 coord, uint direction, uint materialId,
                                  uint chunkIndex, uint localQuadIndex)
{
    PackedVoxelQuad16 quad;
    quad.pos_size_dir_lod = pack_pos_size_dir_lod(coord, direction);
    quad.material_flags = materialId & 0xfffffu;
    quad.light_ao_uv = 0u;
    quad.chunk_batch_meta = (localQuadIndex & 0xffffu) | ((chunkIndex & 0xffu) << 16u);
    return quad;
}

void emit_direction(GpuChunkHeader header, uint chunkIndex, uint chunkBase,
                    uint direction, inout uint localIndex,
                    inout uint materialSum, inout uint checksum)
{
    uint outputBase = greedyQuadOffsets[chunkIndex];
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
                uint3 coord = face_coord(direction, slice, u, v);
                PackedVoxelQuad16 quad =
                    pack_probe_quad(coord, direction, materialId, chunkIndex, localIndex);
                packedQuads[outputBase + localIndex] = quad;
                checksum ^= quad.pos_size_dir_lod * 16777619u;
                checksum ^= quad.material_flags * 2166136261u;
                ++localIndex;
                materialSum += materialId;
            }
        }
    }
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint chunkIndex = dispatchThreadId.x;
    GpuChunkHeader header = chunkHeaders[chunkIndex];
    uint chunkBase = chunkIndex * ProbeChunkVoxelCount;
    uint emitted = 0u;
    uint materialSum = 0u;
    uint checksum = 0u;
    for (uint direction = 0u; direction < 6u; ++direction)
    {
        emit_direction(header, chunkIndex, chunkBase, direction, emitted,
                       materialSum, checksum);
    }
    emittedQuadCounts[chunkIndex] = emitted;
    emittedMaterialSums[chunkIndex] = materialSum;
    emittedChecksums[chunkIndex] = checksum ^ greedyQuadCounts[chunkIndex];
}
