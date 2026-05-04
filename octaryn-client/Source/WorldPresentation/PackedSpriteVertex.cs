using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal static class PackedSpriteVertex
{
    private const int DirectionCount = 6;
    private const int OcclusionOffset = 0;
    private const int DirectionOffset = 1;
    private const int XOffset = 5;
    private const int YOffset = 10;
    private const int ZOffset = 18;
    private const int UOffset = 23;
    private const int VOffset = 24;
    private const int AtlasLayerOffset = 25;

    private const uint OcclusionMask = 0x1;
    private const uint DirectionMask = 0xF;
    private const uint XMask = 0x1F;
    private const uint YMask = 0xFF;
    private const uint ZMask = 0x1F;
    private const uint UMask = 0x1;
    private const uint VMask = 0x1;
    private const uint AtlasLayerMask = 0x3F;

    private static readonly int[,,] Texcoords =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
        {{0, 1}, {0, 0}, {1, 1}, {1, 0}}
    };

    public static uint Pack(SpriteMeshFace face, BlockRenderRules rules, int vertex)
    {
        if (vertex is < 0 or > 3)
        {
            throw new ArgumentOutOfRangeException(nameof(vertex), vertex, "Sprite vertex must be 0..3");
        }

        var packedDirection = PackedMeshDirectionMap.FromDirection(face.Direction);
        var directionIndex = (int)packedDirection;
        var properties = rules.Properties(face.Block);
        uint packed = 0;
        packed = PackField(packed, properties.HasOcclusion ? 1u : 0u, OcclusionOffset, OcclusionMask);
        packed = PackField(packed, (uint)(DirectionCount + directionIndex), DirectionOffset, DirectionMask);
        packed = PackField(packed, (uint)face.X, XOffset, XMask);
        packed = PackField(packed, (uint)face.Y, YOffset, YMask);
        packed = PackField(packed, (uint)face.Z, ZOffset, ZMask);
        packed = PackField(packed, (uint)Texcoords[directionIndex, vertex, 0], UOffset, UMask);
        packed = PackField(packed, (uint)Texcoords[directionIndex, vertex, 1], VOffset, VMask);
        packed = PackField(packed, (uint)rules.AtlasLayer(face.Block, Direction.PositiveZ), AtlasLayerOffset, AtlasLayerMask);
        return packed;
    }

    public static bool HasOcclusion(uint packed)
    {
        return ((packed >> OcclusionOffset) & OcclusionMask) != 0;
    }

    public static int PackedDirection(uint packed)
    {
        return (int)((packed >> DirectionOffset) & DirectionMask);
    }

    public static int X(uint packed)
    {
        return (int)((packed >> XOffset) & XMask);
    }

    public static int Y(uint packed)
    {
        return (int)((packed >> YOffset) & YMask);
    }

    public static int Z(uint packed)
    {
        return (int)((packed >> ZOffset) & ZMask);
    }

    public static int AtlasLayer(uint packed)
    {
        return (int)((packed >> AtlasLayerOffset) & AtlasLayerMask);
    }

    public static int U(uint packed)
    {
        return (int)((packed >> UOffset) & UMask);
    }

    public static int V(uint packed)
    {
        return (int)((packed >> VOffset) & VMask);
    }

    private static uint PackField(uint packed, uint value, int offset, uint mask)
    {
        if (value > mask)
        {
            throw new ArgumentOutOfRangeException(nameof(value), value, "Packed sprite vertex field is too large");
        }

        return packed | ((value & mask) << offset);
    }
}
