using System.Runtime.InteropServices;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeBlockPosition(int x, int y, int z)
{
    public readonly int X = x;
    public readonly int Y = y;
    public readonly int Z = z;

    public static NativeBlockPosition FromBlockPosition(BlockPosition position)
    {
        return new NativeBlockPosition(position.X, position.Y, position.Z);
    }

    public BlockPosition ToBlockPosition()
    {
        return new BlockPosition(X, Y, Z);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeChunkPosition(int x, int y, int z)
{
    public readonly int X = x;
    public readonly int Y = y;
    public readonly int Z = z;

    public ChunkPosition ToChunkPosition()
    {
        return new ChunkPosition(X, Y, Z);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeBlockEdit(NativeBlockPosition position, ushort block)
{
    public readonly NativeBlockPosition Position = position;
    public readonly ushort Block = block;

    public static NativeBlockEdit FromBlockEdit(BlockEdit edit)
    {
        return new NativeBlockEdit(
            NativeBlockPosition.FromBlockPosition(edit.Position),
            edit.Block.Value);
    }

    public BlockEdit ToBlockEdit()
    {
        return new BlockEdit(Position.ToBlockPosition(), new BlockId(Block));
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeBlockEditResult(uint applied, uint changed, NativeBlockEdit edit)
{
    public readonly uint Applied = applied;
    public readonly uint Changed = changed;
    public readonly NativeBlockEdit Edit = edit;

    public BlockEditResult ToBlockEditResult()
    {
        if (Changed != 0)
        {
            return BlockEditResult.ChangedEdit(Edit.ToBlockEdit());
        }

        return Applied != 0 ? BlockEditResult.Unchanged : default;
    }
}

internal enum NativeClientBlockCommandSubmitReason : uint
{
    Accepted = 0,
    Capacity = 1,
    RejectedCommand = 2,
    NativeSubmit = 3,
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeClientBlockCommandSubmitReport(
    int result,
    uint rejectedIndex,
    NativeClientBlockCommandSubmitReason reason,
    uint requestedCount,
    ulong pendingBefore,
    ulong pendingAfter)
{
    public readonly int Result = result;
    public readonly uint RejectedIndex = rejectedIndex;
    public readonly NativeClientBlockCommandSubmitReason Reason = reason;
    public readonly uint RequestedCount = requestedCount;
    public readonly ulong PendingBefore = pendingBefore;
    public readonly ulong PendingAfter = pendingAfter;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeClientBlockCommandDrainReport(
    int applied,
    ulong pendingAfter)
{
    public readonly int Applied = applied;
    public readonly ulong PendingAfter = pendingAfter;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeBlockChangeSnapshotDrainReport(
    int result,
    ulong requestedCapacity,
    ulong pendingBefore,
    uint written)
{
    public readonly int Result = result;
    public readonly ulong RequestedCapacity = requestedCapacity;
    public readonly ulong PendingBefore = pendingBefore;
    public readonly uint Written = written;
}
