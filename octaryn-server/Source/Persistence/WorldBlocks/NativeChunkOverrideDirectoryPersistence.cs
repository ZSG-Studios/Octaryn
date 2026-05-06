using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
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
}
