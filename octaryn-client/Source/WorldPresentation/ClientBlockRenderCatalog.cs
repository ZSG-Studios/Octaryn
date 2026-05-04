using System.Text.Json;
using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal sealed class ClientBlockRenderCatalog
{
    private readonly ClientBlockRenderProperties[] _properties;
    private readonly int[,] _atlasLayers;

    private ClientBlockRenderCatalog(ClientBlockRenderProperties[] properties, int[,] atlasLayers)
    {
        _properties = properties;
        _atlasLayers = atlasLayers;
    }

    public static ClientBlockRenderCatalog LoadBundledCatalog()
    {
        return Load(ClientBlockCatalogPath.Resolve());
    }

    public static ClientBlockRenderCatalog Empty()
    {
        var properties = new[] { ClientBlockRenderProperties.Air };
        return new ClientBlockRenderCatalog(properties, new int[properties.Length, 6]);
    }

    public static ClientBlockRenderCatalog Load(string path)
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
        var properties = new ClientBlockRenderProperties[blocks.GetArrayLength()];
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

        return new ClientBlockRenderCatalog(properties, atlasLayers);
    }

    public ClientBlockRenderProperties Properties(BlockId block)
    {
        return block.Value < _properties.Length ? _properties[block.Value] : ClientBlockRenderProperties.Air;
    }

    public int AtlasLayer(BlockId block, Direction direction)
    {
        if (block.Value >= _properties.Length)
        {
            return 0;
        }

        return _atlasLayers[block.Value, ClientPackedMeshDirectionMap.ToOldDirectionIndex(direction)];
    }

    private static ClientBlockRenderProperties CreateProperties(JsonElement block, int blockIndex)
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
            "water" => ClientBlockRenderKind.Water,
            "lava" => ClientBlockRenderKind.Lava,
            _ when blockIndex == 0 => ClientBlockRenderKind.Empty,
            _ when sprite => ClientBlockRenderKind.Sprite,
            _ when opaque => ClientBlockRenderKind.OpaqueCube,
            _ when solid => ClientBlockRenderKind.TransparentCube,
            _ => ClientBlockRenderKind.Hidden
        };

        return new ClientBlockRenderProperties(
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
