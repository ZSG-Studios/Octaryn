using System.Runtime.InteropServices;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static NativePersistenceChunkColumnPlan PlanChunkColumns(IReadOnlyList<BlockEdit> edits)
    {
        var nativeEdits = edits.Select(NativePersistenceBlockEdit.FromBlockEdit).ToArray();
        return PlanChunkColumns(nativeEdits);
    }

    public static NativePersistenceChunkColumnPlan PlanChunkColumns(ReadOnlySpan<NativePersistenceBlockEdit> nativeEdits)
    {
        var counts = default(NativePersistencePlanCounts);
        fixed (NativePersistenceBlockEdit* editPointer = nativeEdits)
        {
            var countResult = s_planChunkColumnsCount(
                editPointer,
                (uint)nativeEdits.Length,
                &counts);
            if (countResult != 0)
            {
                throw new InvalidOperationException("Native world persistence chunk-column count failed.");
            }

            var columns = new NativePersistenceChunkColumn[counts.ColumnCount];
            var orderedNativeEdits = new NativePersistenceBlockEdit[counts.BlockCount];
            var written = default(NativePersistencePlanCounts);
            fixed (NativePersistenceChunkColumn* columnPointer = columns)
            fixed (NativePersistenceBlockEdit* orderedEditPointer = orderedNativeEdits)
            {
                var fillResult = s_planChunkColumnsFill(
                    editPointer,
                    (uint)nativeEdits.Length,
                    columnPointer,
                    counts.ColumnCount,
                    orderedEditPointer,
                    counts.BlockCount,
                    &written);
                if (fillResult != 0)
                {
                    throw new InvalidOperationException("Native world persistence chunk-column fill failed.");
                }
            }

            return new NativePersistenceChunkColumnPlan(columns, orderedNativeEdits);
        }
    }

    public static NativePersistenceChunkOverrideDirectoryScan ScanChunkOverrideDirectory(
        string directory,
        string aggregatePath)
    {
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
        var aggregatePointer = Marshal.StringToCoTaskMemUTF8(aggregatePath);
        try
        {
            var scan = default(NativePersistenceChunkOverrideDirectoryScan);
            var result = s_scanChunkOverrideDirectory(directoryPointer, aggregatePointer, &scan);
            if (result != 0)
            {
                throw new IOException("Native chunk-column override directory scan failed.");
            }

            return scan;
        }
        finally
        {
            Marshal.FreeCoTaskMem(directoryPointer);
            Marshal.FreeCoTaskMem(aggregatePointer);
        }
    }

    public static NativePersistenceBlockEdit[] ReadChunkOverrideDirectory(string directory)
    {
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            uint blockCount = 0;
            if (s_readChunkOverrideDirectoryCount(directoryPointer, &blockCount) != 0 ||
                blockCount > int.MaxValue)
            {
                return [];
            }

            var edits = new NativePersistenceBlockEdit[checked((int)blockCount)];
            fixed (NativePersistenceBlockEdit* editPointer = edits)
            {
                uint written = 0;
                var result = s_readChunkOverrideDirectoryFill(
                    directoryPointer,
                    editPointer,
                    blockCount,
                    &written);
                return result == 0 && written == blockCount ? edits : [];
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }

    public static int CountChunkOverrideDirectoryBlocks(string directory)
    {
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            uint blockCount = 0;
            return s_readChunkOverrideDirectoryCount(directoryPointer, &blockCount) == 0 &&
                blockCount <= int.MaxValue
                    ? checked((int)blockCount)
                    : 0;
        }
        finally
        {
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }

    public static void PruneStaleChunkOverrideFiles(
        string directory,
        IReadOnlyList<NativePersistenceChunkColumn> plannedColumns)
    {
        var columns = plannedColumns.ToArray();
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            fixed (NativePersistenceChunkColumn* columnPointer = columns)
            {
                uint removed = 0;
                var result = s_pruneStaleChunkOverrideFiles(
                    directoryPointer,
                    columnPointer,
                    (uint)columns.Length,
                    &removed);
                if (result != 0)
                {
                    throw new IOException("Native chunk-column stale override pruning failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }

    public static void WriteChunkOverrideDirectory(
        string directory,
        ReadOnlySpan<NativePersistenceChunkColumn> columns,
        ReadOnlySpan<NativePersistenceBlockEdit> orderedEdits)
    {
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            fixed (NativePersistenceChunkColumn* columnPointer = columns)
            fixed (NativePersistenceBlockEdit* editPointer = orderedEdits)
            {
                var result = s_writeChunkOverrideDirectory(
                    directoryPointer,
                    columnPointer,
                    (uint)columns.Length,
                    editPointer,
                    (uint)orderedEdits.Length);
                if (result != 0)
                {
                    throw new IOException("Native chunk-column override directory write failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }
}
