using System.Runtime.InteropServices;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceBlockPosition(int x, int y, int z)
{
    public readonly int X = x;
    public readonly int Y = y;
    public readonly int Z = z;

    public static NativePersistenceBlockPosition FromBlockPosition(BlockPosition position)
    {
        return new NativePersistenceBlockPosition(position.X, position.Y, position.Z);
    }

    public BlockPosition ToBlockPosition()
    {
        return new BlockPosition(X, Y, Z);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceBlockEdit(NativePersistenceBlockPosition position, ushort block)
{
    public readonly NativePersistenceBlockPosition Position = position;
    public readonly ushort Block = block;

    public static NativePersistenceBlockEdit FromBlockEdit(BlockEdit edit)
    {
        return new NativePersistenceBlockEdit(
            NativePersistenceBlockPosition.FromBlockPosition(edit.Position),
            edit.Block.Value);
    }

    public BlockEdit ToBlockEdit()
    {
        return new BlockEdit(Position.ToBlockPosition(), new BlockId(Block));
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceChunkColumn(int originX, int originZ, uint blockOffset, uint blockCount)
{
    public readonly int OriginX = originX;
    public readonly int OriginZ = originZ;
    public readonly uint BlockOffset = blockOffset;
    public readonly uint BlockCount = blockCount;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistencePlanCounts(uint columnCount, uint blockCount)
{
    public readonly uint ColumnCount = columnCount;
    public readonly uint BlockCount = blockCount;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistencePlayerState(
    float x,
    float y,
    float z,
    float pitch,
    float yaw,
    ushort block)
{
    public readonly float X = x;
    public readonly float Y = y;
    public readonly float Z = z;
    public readonly float Pitch = pitch;
    public readonly float Yaw = yaw;
    public readonly ushort Block = block;
}
