using Octaryn.Basegame.Content.Fluids;
using Octaryn.Shared.World;

namespace Octaryn.Basegame.Content.Blocks;

public static class BlockCatalog
{
    public const ushort KnownBlockCount = 39;

    private static readonly ushort[] GrassSupportedBlockIds =
    [
        9,
        10,
        11,
        12,
        13
    ];

    private static readonly ushort[] PlaceableBlockIds =
    [
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        9,
        10,
        11,
        12,
        13,
        14,
        22,
        23,
        24,
        25,
        26,
        27,
        28,
        29,
        30,
        31
    ];

    private static readonly ushort[] SolidBaseSupportedBlockIds =
    [
        22,
        23,
        24,
        25,
        26,
        27,
        28
    ];

    private static readonly ushort[] SolidBlockIds =
    [
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        29,
        30
    ];

    private static readonly ushort[] TargetableBlockIds =
    [
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        9,
        10,
        11,
        12,
        13,
        22,
        23,
        24,
        25,
        26,
        27,
        28,
        29,
        30
    ];

    private static readonly ushort[] FluidKindValues =
    [
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        2,
        2,
        2,
        2,
        2,
        2,
        2,
        2
    ];

    private static readonly int[] FluidLevels =
    [
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        0,
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        0,
        1,
        2,
        3,
        4,
        5,
        6,
        7
    ];

    private static readonly ushort[] FluidSourceBlockIds =
    [
        14,
        31
    ];

    private static readonly int[] SkylightOpacities =
    [
        0,
        15,
        15,
        15,
        15,
        15,
        15,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        2,
        2,
        2,
        2,
        2,
        2,
        2,
        2,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        15,
        0,
        2,
        2,
        2,
        2,
        2,
        2,
        2,
        2
    ];

    public static BlockId Dirt => new(2);

    public static BlockId Grass => new(1);

    public static BlockId Sand => new(3);

    public static BlockId Snow => new(4);

    public static BlockId Stone => new(5);

    public static BlockId Log => new(6);

    public static BlockId Leaves => new(7);

    public static BlockId Cloud => new(8);

    public static BlockId Bush => new(9);

    public static BlockId Bluebell => new(10);

    public static BlockId Gardenia => new(11);

    public static BlockId Rose => new(12);

    public static BlockId Lavender => new(13);

    public static BlockId RedTorch => new(22);

    public static BlockId GreenTorch => new(23);

    public static BlockId BlueTorch => new(24);

    public static BlockId YellowTorch => new(25);

    public static BlockId CyanTorch => new(26);

    public static BlockId MagentaTorch => new(27);

    public static BlockId WhiteTorch => new(28);

    public static BlockId Planks => new(29);

    public static BlockId Glass => new(30);

    public static BlockId DefaultSelectedBlock => new(25);

    public static BlockId WaterSource => new(14);

    public static BlockId WaterLevelOne => new(15);

    public static BlockId WaterLevelSeven => new(21);

    public static BlockId LavaSource => new(31);

    public static BlockId LavaLevelOne => new(32);

    public static BlockId LavaLevelSeven => new(38);

    public static bool IsKnown(BlockId block)
    {
        return block.Value < KnownBlockCount;
    }

    public static bool IsPlaceable(BlockId block)
    {
        return IndexOf(PlaceableBlockIds, block.Value) >= 0;
    }

    public static bool IsSolid(BlockId block)
    {
        return IndexOf(SolidBlockIds, block.Value) >= 0;
    }

    public static bool IsTargetable(BlockId block)
    {
        return IndexOf(TargetableBlockIds, block.Value) >= 0;
    }

    public static bool IsFluid(BlockId block)
    {
        return GetFluidKind(block) != FluidKind.None;
    }

    public static bool IsFluidSource(BlockId block)
    {
        return IndexOf(FluidSourceBlockIds, block.Value) >= 0;
    }

    public static bool IsWater(BlockId block)
    {
        return GetFluidKind(block) == FluidKind.Water;
    }

    public static bool IsLava(BlockId block)
    {
        return GetFluidKind(block) == FluidKind.Lava;
    }

    public static FluidKind GetFluidKind(BlockId block)
    {
        return block.Value < FluidKindValues.Length
            ? (FluidKind)FluidKindValues[block.Value]
            : FluidKind.None;
    }

    public static int FluidLevel(BlockId block)
    {
        return block.Value < FluidLevels.Length ? FluidLevels[block.Value] : -1;
    }

    public static int SkylightOpacity(BlockId block)
    {
        return block.Value < SkylightOpacities.Length ? SkylightOpacities[block.Value] : 0;
    }

    public static BlockId MakeWater(int level)
    {
        return new BlockId(ClampedFluidBlockId(14, 21, level));
    }

    public static BlockId MakeLava(int level)
    {
        return new BlockId(ClampedFluidBlockId(31, 38, level));
    }

    public static BlockId MakeFluid(FluidKind kind, int level)
    {
        return kind switch
        {
            FluidKind.Water => MakeWater(level),
            FluidKind.Lava => MakeLava(level),
            _ => BlockId.Air
        };
    }

    public static bool RequiresGrass(BlockId block)
    {
        return IndexOf(GrassSupportedBlockIds, block.Value) >= 0;
    }

    public static bool RequiresSolidBase(BlockId block)
    {
        return IndexOf(SolidBaseSupportedBlockIds, block.Value) >= 0;
    }

    private static ushort ClampedFluidBlockId(ushort sourceId, ushort maxLevelId, int level)
    {
        if (level <= 0)
        {
            return sourceId;
        }

        if (level >= 7)
        {
            return maxLevelId;
        }

        return (ushort)(sourceId + level);
    }

    private static int IndexOf(ushort[] values, ushort value)
    {
        for (var index = 0; index < values.Length; index++)
        {
            if (values[index] == value)
            {
                return index;
            }
        }

        return -1;
    }
}
