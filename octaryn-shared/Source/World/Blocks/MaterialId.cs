namespace Octaryn.Shared.World;

public readonly record struct MaterialId(ushort Value)
{
    public static MaterialId None => new(0);
}
