using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal sealed class ChunkMeshPlanner
{
    private readonly BlockRenderRules _rules;

    public ChunkMeshPlanner(BlockRenderRules rules)
    {
        _rules = rules;
    }

    public ChunkMeshPlan Build(ChunkNeighborhoodSnapshot snapshot)
    {
        var cubeFaces = new List<CubeMeshFace>();
        var spriteFaces = new List<SpriteMeshFace>();
        var fluidBlocks = new List<FluidMeshBlock>();

        for (var x = 0; x < ChunkNeighborhoodSnapshot.Width; x++)
        for (var y = 0; y < ChunkNeighborhoodSnapshot.Height; y++)
        for (var z = 0; z < ChunkNeighborhoodSnapshot.Depth; z++)
        {
            var block = snapshot.LocalBlock(1, 1, x, y, z);
            var properties = _rules.Properties(block);
            switch (properties.Kind)
            {
                case BlockRenderKind.Empty:
                case BlockRenderKind.Hidden:
                    break;
                case BlockRenderKind.Sprite:
                    AppendSpriteFaces(spriteFaces, block, x, y, z);
                    break;
                case BlockRenderKind.Water:
                case BlockRenderKind.Lava:
                    fluidBlocks.Add(new FluidMeshBlock(block, properties.Kind, x, y, z, properties.FluidLevel));
                    break;
                case BlockRenderKind.OpaqueCube:
                case BlockRenderKind.TransparentCube:
                    AppendCubeFaces(cubeFaces, snapshot, block, properties.Kind, x, y, z);
                    break;
            }
        }

        return new ChunkMeshPlan(cubeFaces, spriteFaces, fluidBlocks);
    }

    private void AppendSpriteFaces(
        List<SpriteMeshFace> spriteFaces,
        BlockId block,
        int x,
        int y,
        int z)
    {
        foreach (var direction in _rules.SpriteFaceDirections(block))
        {
            spriteFaces.Add(new SpriteMeshFace(block, x, y, z, direction));
        }
    }

    private void AppendCubeFaces(
        List<CubeMeshFace> cubeFaces,
        ChunkNeighborhoodSnapshot snapshot,
        BlockId block,
        BlockRenderKind kind,
        int x,
        int y,
        int z)
    {
        foreach (var direction in _rules.VisibleCubeFaces(snapshot, x, y, z))
        {
            cubeFaces.Add(new CubeMeshFace(block, kind, x, y, z, direction));
        }
    }
}
