StructuredBuffer<uint> greedyQuadCounts : register(t0, space0);
RWStructuredBuffer<uint> greedyQuadOffsets : register(u1, space0);
RWStructuredBuffer<uint> greedyTotalQuads : register(u2, space0);

static const uint ProbeChunkCount = 4u;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x != 0u)
    {
        return;
    }
    uint runningTotal = 0u;
    for (uint index = 0u; index < ProbeChunkCount; ++index)
    {
        greedyQuadOffsets[index] = runningTotal;
        runningTotal += greedyQuadCounts[index];
    }
    greedyTotalQuads[0] = runningTotal;
}
