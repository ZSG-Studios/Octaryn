using Octaryn.Shared.World;

namespace Octaryn.Basegame.Content.Blocks;

public static partial class BlockCatalog
{
    private static readonly BlockAtlasFaceLayers[] AtlasFaceLayers =
    [
        new BlockAtlasFaceLayers(0, 0, 0, 0, 0, 0),
        new BlockAtlasFaceLayers(2, 2, 2, 2, 1, 3),
        new BlockAtlasFaceLayers(3, 3, 3, 3, 3, 3),
        new BlockAtlasFaceLayers(5, 5, 5, 5, 5, 5),
        new BlockAtlasFaceLayers(6, 6, 6, 6, 6, 6),
        new BlockAtlasFaceLayers(4, 4, 4, 4, 4, 4),
        new BlockAtlasFaceLayers(8, 8, 8, 8, 7, 7),
        new BlockAtlasFaceLayers(10, 10, 10, 10, 10, 10),
        new BlockAtlasFaceLayers(9, 9, 9, 9, 9, 9),
        new BlockAtlasFaceLayers(15, 15, 15, 15, 15, 15),
        new BlockAtlasFaceLayers(13, 13, 13, 13, 13, 13),
        new BlockAtlasFaceLayers(12, 12, 12, 12, 12, 12),
        new BlockAtlasFaceLayers(11, 11, 11, 11, 11, 11),
        new BlockAtlasFaceLayers(14, 14, 14, 14, 14, 14),
        new BlockAtlasFaceLayers(16, 16, 16, 16, 16, 16),
        new BlockAtlasFaceLayers(16, 16, 16, 16, 16, 16),
        new BlockAtlasFaceLayers(16, 16, 16, 16, 16, 16),
        new BlockAtlasFaceLayers(16, 16, 16, 16, 16, 16),
        new BlockAtlasFaceLayers(16, 16, 16, 16, 16, 16),
        new BlockAtlasFaceLayers(16, 16, 16, 16, 16, 16),
        new BlockAtlasFaceLayers(16, 16, 16, 16, 16, 16),
        new BlockAtlasFaceLayers(16, 16, 16, 16, 16, 16),
        new BlockAtlasFaceLayers(17, 17, 17, 17, 17, 17),
        new BlockAtlasFaceLayers(18, 18, 18, 18, 18, 18),
        new BlockAtlasFaceLayers(19, 19, 19, 19, 19, 19),
        new BlockAtlasFaceLayers(20, 20, 20, 20, 20, 20),
        new BlockAtlasFaceLayers(21, 21, 21, 21, 21, 21),
        new BlockAtlasFaceLayers(22, 22, 22, 22, 22, 22),
        new BlockAtlasFaceLayers(23, 23, 23, 23, 23, 23),
        new BlockAtlasFaceLayers(24, 24, 24, 24, 24, 24),
        new BlockAtlasFaceLayers(25, 25, 25, 25, 25, 25),
        new BlockAtlasFaceLayers(27, 27, 27, 27, 27, 27),
        new BlockAtlasFaceLayers(27, 27, 27, 27, 27, 27),
        new BlockAtlasFaceLayers(27, 27, 27, 27, 27, 27),
        new BlockAtlasFaceLayers(27, 27, 27, 27, 27, 27),
        new BlockAtlasFaceLayers(27, 27, 27, 27, 27, 27),
        new BlockAtlasFaceLayers(27, 27, 27, 27, 27, 27),
        new BlockAtlasFaceLayers(27, 27, 27, 27, 27, 27),
        new BlockAtlasFaceLayers(27, 27, 27, 27, 27, 27)
    ];

    public static BlockAtlasFaceLayers AtlasLayers(BlockId block)
    {
        return block.Value < AtlasFaceLayers.Length
            ? AtlasFaceLayers[block.Value]
            : default;
    }

    public static ushort AtlasLayer(BlockId block, BlockFace face)
    {
        return AtlasLayers(block).LayerFor(face);
    }
}
