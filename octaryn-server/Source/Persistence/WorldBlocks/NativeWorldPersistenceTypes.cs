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
internal readonly struct NativePersistenceChunkOverrideBlock(int bx, int by, int bz, ushort block)
{
    public readonly int Bx = bx;
    public readonly int By = by;
    public readonly int Bz = bz;
    public readonly ushort Block = block;

    public static NativePersistenceChunkOverrideBlock FromBlock(ChunkColumnBlockOverrideRecord block)
    {
        return new NativePersistenceChunkOverrideBlock(block.Bx, block.By, block.Bz, block.Block);
    }

    public ChunkColumnBlockOverrideRecord ToBlock()
    {
        return new ChunkColumnBlockOverrideRecord(Bx, By, Bz, Block);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceChunkOverrideFile(uint version, int cx, int cz, uint blockCount)
{
    public readonly uint Version = version;
    public readonly int Cx = cx;
    public readonly int Cz = cz;
    public readonly uint BlockCount = blockCount;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceSaveImportChunk(uint version, int cx, int cz, uint blockOffset, uint blockCount)
{
    public readonly uint Version = version;
    public readonly int Cx = cx;
    public readonly int Cz = cz;
    public readonly uint BlockOffset = blockOffset;
    public readonly uint BlockCount = blockCount;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceWorldBlockOverrideFile(uint version, uint blockCount)
{
    public readonly uint Version = version;
    public readonly uint BlockCount = blockCount;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistencePlanCounts(uint columnCount, uint blockCount)
{
    public readonly uint ColumnCount = columnCount;
    public readonly uint BlockCount = blockCount;
}

internal sealed class NativePersistenceChunkColumnPlan(
    NativePersistenceChunkColumn[] columns,
    NativePersistenceBlockEdit[] orderedEdits)
{
    public NativePersistenceChunkColumn[] Columns { get; } = columns;

    public NativePersistenceBlockEdit[] OrderedEdits { get; } = orderedEdits;

    public NativePersistenceBlockEdit[] NativeEditsFor(NativePersistenceChunkColumn column)
    {
        return OrderedEdits
            .Skip(checked((int)column.BlockOffset))
            .Take(checked((int)column.BlockCount))
            .ToArray();
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceChunkOverrideDirectoryScan(
    uint currentFilesAtLeastAsNewAs,
    uint fileCount,
    uint blockCount)
{
    public readonly uint CurrentFilesAtLeastAsNewAs = currentFilesAtLeastAsNewAs;
    public readonly uint FileCount = fileCount;
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

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistencePlayerFileEntry(int playerId, NativePersistencePlayerState state)
{
    public readonly int PlayerId = playerId;
    public readonly NativePersistencePlayerState State = state;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceWorldTimeState(uint version, ulong dayIndex, double secondsOfDay)
{
    public readonly uint Version = version;
    public readonly ulong DayIndex = dayIndex;
    public readonly double SecondsOfDay = secondsOfDay;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceWorldMetadata(
    uint saveExists,
    uint hasWorldTime,
    uint hasPlayerData,
    uint hasWorldData,
    int playerCount,
    int chunkOverrideCount)
{
    public readonly uint SaveExists = saveExists;
    public readonly uint HasWorldTime = hasWorldTime;
    public readonly uint HasPlayerData = hasPlayerData;
    public readonly uint HasWorldData = hasWorldData;
    public readonly int PlayerCount = playerCount;
    public readonly int ChunkOverrideCount = chunkOverrideCount;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistenceSaveExportBundleCounts(
    uint hasWorldTime,
    uint playerCount,
    uint chunkCount,
    uint blockCount)
{
    public readonly uint HasWorldTime = hasWorldTime;
    public readonly uint PlayerCount = playerCount;
    public readonly uint ChunkCount = chunkCount;
    public readonly uint BlockCount = blockCount;
}
