namespace Octaryn.Shared.World;

public readonly record struct BlockId(ushort Value)
{
    public static BlockId Air => new(0);
}
