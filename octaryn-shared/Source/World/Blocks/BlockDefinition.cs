namespace Octaryn.Shared.World;

public readonly record struct BlockDefinition(
    BlockId Id,
    string Name,
    bool IsSolid,
    bool IsOpaque);
