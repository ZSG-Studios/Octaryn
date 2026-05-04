using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal readonly record struct FluidMeshBlock(
    BlockId Block,
    BlockRenderKind Kind,
    int X,
    int Y,
    int Z,
    int Level);
