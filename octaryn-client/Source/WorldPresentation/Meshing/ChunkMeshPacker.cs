namespace Octaryn.Client.WorldPresentation;

internal sealed class ChunkMeshPacker
{
    private readonly BlockRenderRules _rules;

    public ChunkMeshPacker(BlockRenderRules rules)
    {
        _rules = rules;
    }

    public PackedChunkMesh Pack(ChunkMeshPlan plan)
    {
        var opaqueFaces = new List<ulong>();
        var transparentFaces = new List<ulong>();
        var spriteVertices = new List<uint>();

        foreach (var face in plan.CubeFaces)
        {
            var packed = PackedCubeFace.Pack(face, _rules);
            if (face.Kind == BlockRenderKind.TransparentCube)
            {
                transparentFaces.Add(packed);
            }
            else
            {
                opaqueFaces.Add(packed);
            }
        }

        foreach (var face in plan.SpriteFaces)
        {
            for (var vertex = 0; vertex < 4; vertex++)
            {
                spriteVertices.Add(PackedSpriteVertex.Pack(face, _rules, vertex));
            }
        }

        return new PackedChunkMesh(opaqueFaces, transparentFaces, spriteVertices, plan.FluidBlocks);
    }
}
