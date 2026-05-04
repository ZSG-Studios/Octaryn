using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal readonly record struct CubeMeshFace(
    BlockId Block,
    BlockRenderKind Kind,
    int X,
    int Y,
    int Z,
    Direction Direction);
