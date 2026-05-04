namespace Octaryn.Client.WorldPresentation;

internal sealed class ChunkMeshPlan
{
    public ChunkMeshPlan(
        IReadOnlyList<CubeMeshFace> cubeFaces,
        IReadOnlyList<SpriteMeshFace> spriteFaces,
        IReadOnlyList<FluidMeshBlock> fluidBlocks)
    {
        CubeFaces = cubeFaces;
        SpriteFaces = spriteFaces;
        FluidBlocks = fluidBlocks;
    }

    public IReadOnlyList<CubeMeshFace> CubeFaces { get; }

    public IReadOnlyList<SpriteMeshFace> SpriteFaces { get; }

    public IReadOnlyList<FluidMeshBlock> FluidBlocks { get; }
}
