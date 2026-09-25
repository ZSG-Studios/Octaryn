namespace Octaryn.Shared.World;

public readonly record struct ChunkColumnRequest(
    ChunkColumnPosition Center,
    uint Radius);
