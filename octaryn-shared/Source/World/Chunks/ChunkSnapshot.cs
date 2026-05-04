namespace Octaryn.Shared.World;

public readonly record struct ChunkSnapshot(
    ChunkPosition Position,
    IReadOnlyList<BlockId> Blocks);
