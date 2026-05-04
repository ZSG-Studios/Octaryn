namespace Octaryn.Client.WorldPresentation;

internal static class PackedCubeFace
{
    public const ushort UnsetChunkSlot = 0x1FFF;

    private const int XOffset = 0;
    private const int YOffset = 5;
    private const int ZOffset = 13;
    private const int DirectionOffset = 18;
    private const int UExtentOffset = 21;
    private const int VExtentOffset = 29;
    private const int AtlasLayerOffset = 37;
    private const int OcclusionOffset = 43;
    private const int ChunkSlotOffset = 44;
    private const int WaterLevelOffset = 57;
    private const int WaterFlagOffset = 60;
    private const int WaterBaseHeightOffset = 61;

    private const ulong XMask = 0x1F;
    private const ulong YMask = 0xFF;
    private const ulong ZMask = 0x1F;
    private const ulong DirectionMask = 0x7;
    private const ulong UExtentMask = 0xFF;
    private const ulong VExtentMask = 0xFF;
    private const ulong AtlasLayerMask = 0x3F;
    private const ulong ChunkSlotMask = 0x1FFF;

    public static ulong Pack(
        CubeMeshFace face,
        BlockRenderRules rules,
        int uExtent = 1,
        int vExtent = 1)
    {
        var properties = rules.Properties(face.Block);
        ulong packed = 0;
        packed = PackField(packed, (ulong)face.X, XOffset, XMask);
        packed = PackField(packed, (ulong)face.Y, YOffset, YMask);
        packed = PackField(packed, (ulong)face.Z, ZOffset, ZMask);
        packed = PackField(packed, (ulong)PackedMeshDirectionMap.FromDirection(face.Direction), DirectionOffset, DirectionMask);
        packed = PackField(packed, (ulong)(uExtent - 1), UExtentOffset, UExtentMask);
        packed = PackField(packed, (ulong)(vExtent - 1), VExtentOffset, VExtentMask);
        packed = PackField(packed, (ulong)rules.AtlasLayer(face.Block, face.Direction), AtlasLayerOffset, AtlasLayerMask);
        packed = PackField(packed, properties.HasOcclusion ? 1ul : 0ul, OcclusionOffset, 1);
        packed = PackField(packed, UnsetChunkSlot, ChunkSlotOffset, ChunkSlotMask);
        packed = PackField(packed, 0, WaterLevelOffset, 0x7);
        packed = PackField(packed, 0, WaterFlagOffset, 0x1);
        packed = PackField(packed, 0, WaterBaseHeightOffset, 0x7);
        return packed;
    }

    public static int AtlasLayer(ulong packed)
    {
        return (int)((packed >> AtlasLayerOffset) & AtlasLayerMask);
    }

    public static int X(ulong packed)
    {
        return (int)((packed >> XOffset) & XMask);
    }

    public static int Y(ulong packed)
    {
        return (int)((packed >> YOffset) & YMask);
    }

    public static int Z(ulong packed)
    {
        return (int)((packed >> ZOffset) & ZMask);
    }

    public static int Direction(ulong packed)
    {
        return (int)((packed >> DirectionOffset) & DirectionMask);
    }

    public static int UExtent(ulong packed)
    {
        return (int)(((packed >> UExtentOffset) & UExtentMask) + 1);
    }

    public static int VExtent(ulong packed)
    {
        return (int)(((packed >> VExtentOffset) & VExtentMask) + 1);
    }

    public static int ChunkSlot(ulong packed)
    {
        return (int)((packed >> ChunkSlotOffset) & ChunkSlotMask);
    }

    public static bool HasOcclusion(ulong packed)
    {
        return ((packed >> OcclusionOffset) & 1ul) != 0;
    }

    public static int WaterLevel(ulong packed)
    {
        return (int)((packed >> WaterLevelOffset) & 0x7);
    }

    public static bool IsWater(ulong packed)
    {
        return ((packed >> WaterFlagOffset) & 1ul) != 0;
    }

    public static int WaterBaseHeight(ulong packed)
    {
        return (int)((packed >> WaterBaseHeightOffset) & 0x7);
    }

    private static ulong PackField(ulong packed, ulong value, int offset, ulong mask)
    {
        if (value > mask)
        {
            throw new ArgumentOutOfRangeException(nameof(value), value, "Packed cube face field is too large");
        }

        return packed | ((value & mask) << offset);
    }
}
