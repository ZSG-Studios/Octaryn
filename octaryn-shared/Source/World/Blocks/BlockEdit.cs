namespace Octaryn.Shared.World;

public readonly record struct BlockEdit(
    BlockPosition Position,
    BlockId Block);
