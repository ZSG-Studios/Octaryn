using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal readonly record struct BlockPresentationUpdate(
    BlockPosition Position,
    BlockId Block,
    PresentationChunkKey Chunk);
