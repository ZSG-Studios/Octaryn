namespace Octaryn.Client.WorldPresentation;

internal sealed class PackedChunkMesh
{
    public PackedChunkMesh(
        IReadOnlyList<ulong> opaqueCubeFaces,
        IReadOnlyList<ulong> transparentCubeFaces,
        IReadOnlyList<uint> spriteVertices,
        IReadOnlyList<FluidMeshBlock> fluidBlocks)
    {
        OpaqueCubeFaces = opaqueCubeFaces;
        TransparentCubeFaces = transparentCubeFaces;
        SpriteVertices = spriteVertices;
        FluidBlocks = fluidBlocks;
    }

    public IReadOnlyList<ulong> OpaqueCubeFaces { get; }

    public IReadOnlyList<ulong> TransparentCubeFaces { get; }

    public IReadOnlyList<uint> SpriteVertices { get; }

    public IReadOnlyList<FluidMeshBlock> FluidBlocks { get; }
}
