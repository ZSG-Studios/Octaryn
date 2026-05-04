namespace Octaryn.Server.World.Chunks;

internal sealed record ChunkColumnStream(
    int CenterChunkX,
    int CenterChunkZ,
    uint Radius,
    ChunkWindowPlan Window,
    IReadOnlyList<ChunkColumnStreamColumn> Columns,
    IReadOnlyList<ChunkColumnStreamBlock> Blocks);

internal sealed record ChunkColumnStreamColumn(
    int ChunkX,
    int ChunkZ,
    int OriginX,
    int OriginZ,
    uint BlockOffset,
    uint BlockCount);

internal sealed record ChunkColumnStreamBlock(
    int X,
    int Y,
    int Z,
    ushort Block);
