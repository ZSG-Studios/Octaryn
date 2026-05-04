using System.Text.Json;
using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal sealed class BlockRenderCatalog
{
    private readonly BlockRenderProperties[] _properties;
    private readonly int[,] _atlasLayers;

    private BlockRenderCatalog(BlockRenderProperties[] properties, int[,] atlasLayers)
    {
        _properties = properties;
        _atlasLayers = atlasLayers;
    }

    public static BlockRenderCatalog LoadBundledCatalog()
    {
        return Load(BlockCatalogPath.Resolve());
    }

    public static BlockRenderCatalog Empty()
    {
        var properties = new[] { BlockRenderProperties.Air };
        return new BlockRenderCatalog(properties, new int[properties.Length, 6]);
    }

    public static BlockRenderCatalog Load(string path)
    {
        using var stream = File.OpenRead(path);
        using var document = JsonDocument.Parse(stream);
        var root = document.RootElement;
        var schema = root.GetProperty("schema").GetString();
        if (string.IsNullOrWhiteSpace(schema) || !schema.EndsWith(".blocks.v1", StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"Unsupported block catalog schema in {path}.");
        }

        var blocks = root.GetProperty("blocks");
        var properties = new BlockRenderProperties[blocks.GetArrayLength()];
        var atlasLayers = new int[properties.Length, 6];
        for (var index = 0; index < properties.Length; index++)
        {
            var block = blocks[index];
            var blockId = block.GetProperty("id").GetString();
            if (string.IsNullOrWhiteSpace(blockId))
            {
                throw new InvalidOperationException($"Block catalog entry at index {index} has no stable id.");
            }

            properties[index] = CreateProperties(block, index);
            ReadAtlas(block.GetProperty("atlas"), atlasLayers, index);
        }

        return new BlockRenderCatalog(properties, atlasLayers);
    }

    public BlockRenderProperties Properties(BlockId block)
    {
        return block.Value < _properties.Length ? _properties[block.Value] : BlockRenderProperties.Air;
    }

    public int AtlasLayer(BlockId block, Direction direction)
    {
        if (block.Value >= _properties.Length)
        {
            return 0;
        }

        return _atlasLayers[block.Value, PackedMeshDirectionMap.ToAtlasDirectionIndex(direction)];
    }

    private static BlockRenderProperties CreateProperties(JsonElement block, int blockIndex)
    {
        var blockId = block.GetProperty("id").GetString();
        var fluidKind = block.GetProperty("fluidKind").GetString();
        var fluidLevel = block.GetProperty("fluidLevel").GetInt32();
        var sprite = block.GetProperty("sprite").GetBoolean();
        var opaque = block.GetProperty("opaque").GetBoolean();
        var solid = block.GetProperty("solid").GetBoolean();
        var occlusion = block.GetProperty("occlusion").GetBoolean();
        var requiresSolidBase = block.GetProperty("requiresSolidBase").GetBoolean();

        var kind = fluidKind switch
        {
            "water" => BlockRenderKind.Water,
            "lava" => BlockRenderKind.Lava,
            _ when blockIndex == 0 => BlockRenderKind.Empty,
            _ when sprite => BlockRenderKind.Sprite,
            _ when opaque => BlockRenderKind.OpaqueCube,
            _ when solid => BlockRenderKind.TransparentCube,
            _ => BlockRenderKind.Hidden
        };

        return new BlockRenderProperties(
            kind,
            IsOpaque: opaque,
            HasOcclusion: occlusion,
            IsSprite: sprite,
            IsFluid: fluidKind is "water" or "lava",
            FluidLevel: fluidLevel,
            RequiresSolidBase: requiresSolidBase);
    }

    private static void ReadAtlas(JsonElement atlas, int[,] atlasLayers, int blockIndex)
    {
        atlasLayers[blockIndex, 0] = atlas.GetProperty("north").GetInt32();
        atlasLayers[blockIndex, 1] = atlas.GetProperty("south").GetInt32();
        atlasLayers[blockIndex, 2] = atlas.GetProperty("east").GetInt32();
        atlasLayers[blockIndex, 3] = atlas.GetProperty("west").GetInt32();
        atlasLayers[blockIndex, 4] = atlas.GetProperty("up").GetInt32();
        atlasLayers[blockIndex, 5] = atlas.GetProperty("down").GetInt32();
    }
}
