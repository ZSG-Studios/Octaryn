namespace Octaryn.Shared.World;

public enum BlockRenderPass : byte
{
    None = 0,
    Opaque = 1,
    Cutout = 2,
    Transparent = 3,
    Fluid = 4
}
