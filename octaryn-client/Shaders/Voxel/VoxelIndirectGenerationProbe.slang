#include "VoxelIndirect.slang"
#include "VoxelTypes.slang"

static const uint ProbeChunkCount = 4u;

StructuredBuffer<PackedVoxelQuad16> packedQuads : register(t0, space0);
StructuredBuffer<uint> emittedQuadCounts : register(t1, space0);
StructuredBuffer<uint> greedyQuadOffsets : register(t2, space0);
RWStructuredBuffer<DrawIndexedIndirectCommand> drawCommands : register(u3, space0);
RWStructuredBuffer<uint> drawCount : register(u4, space0);
RWStructuredBuffer<uint> emptyDrawCount : register(u5, space0);
RWStructuredBuffer<uint> totalInstances : register(u6, space0);
RWStructuredBuffer<uint> firstQuadChecksums : register(u7, space0);

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x != 0u)
    {
        return;
    }

    uint compactDraws = 0u;
    uint emptyChunks = 0u;
    uint instances = 0u;
    uint checksum = 0u;
    for (uint chunkIndex = 0u; chunkIndex < ProbeChunkCount; ++chunkIndex)
    {
        uint quadCount = emittedQuadCounts[chunkIndex];
        if (quadCount == 0u)
        {
            ++emptyChunks;
            continue;
        }
        uint quadOffset = greedyQuadOffsets[chunkIndex];
        PackedVoxelQuad16 firstQuad = packedQuads[quadOffset];
        DrawIndexedIndirectCommand command;
        command.indexCount = 6u;
        command.instanceCount = quadCount;
        command.firstIndex = 0u;
        command.vertexOffset = 0;
        command.firstInstance = quadOffset;
        drawCommands[compactDraws] = command;
        checksum ^= firstQuad.pos_size_dir_lod * 16777619u;
        checksum ^= firstQuad.material_flags * 2166136261u;
        checksum ^= quadCount;
        ++compactDraws;
        instances += quadCount;
    }
    drawCount[0] = compactDraws;
    emptyDrawCount[0] = emptyChunks;
    totalInstances[0] = instances;
    firstQuadChecksums[0] = checksum;
}
