namespace Octaryn.Server.World.Chunks;

internal sealed record ServerChunkColumnStream(
    int CenterChunkX,
    int CenterChunkZ,
    uint Radius,
    ServerChunkWindowPlan Window,
    IReadOnlyList<ServerChunkColumnStreamColumn> Columns,
    IReadOnlyList<ServerChunkColumnStreamBlock> Blocks);

internal sealed record ServerChunkColumnStreamColumn(
    int ChunkX,
    int ChunkZ,
    int OriginX,
    int OriginZ,
    uint BlockOffset,
    uint BlockCount);

internal sealed record ServerChunkColumnStreamBlock(
    int X,
    int Y,
    int Z,
    ushort Block);
