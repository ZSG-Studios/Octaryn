using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static NativePersistenceWorldBlockLoadSource SelectWorldBlockLoadSource(
        string chunkDirectory,
        string aggregatePath)
    {
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(chunkDirectory);
        var aggregatePointer = Marshal.StringToCoTaskMemUTF8(aggregatePath);
        try
        {
            var source = default(NativePersistenceWorldBlockLoadSource);
            var result = s_selectWorldBlockLoadSource(directoryPointer, aggregatePointer, &source);
            if (result != 0)
            {
                throw new IOException("Native world-block override load-source selection failed.");
            }

            return source;
        }
        finally
        {
            Marshal.FreeCoTaskMem(directoryPointer);
            Marshal.FreeCoTaskMem(aggregatePointer);
        }
    }

    public static bool TryReadWorldBlockOverrideFile(
        string path,
        out NativePersistenceWorldBlockOverrideFile file,
        out NativePersistenceBlockEdit[] blocks)
    {
        file = default;
        blocks = [];
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceWorldBlockOverrideFile* filePointer = &file)
            {
                if (s_readWorldBlockOverrideFileCount(pathPointer, filePointer) != 0 ||
                    file.BlockCount > int.MaxValue)
                {
                    return false;
                }

                blocks = new NativePersistenceBlockEdit[checked((int)file.BlockCount)];
                fixed (NativePersistenceBlockEdit* blockPointer = blocks)
                {
                    var result = s_readWorldBlockOverrideFileFill(
                        pathPointer,
                        blockPointer,
                        file.BlockCount,
                        filePointer);
                    return result == 0 && file.BlockCount == (uint)blocks.Length;
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WriteWorldBlockOverrideFile(
        string path,
        NativePersistenceWorldBlockOverrideFile file,
        ReadOnlySpan<NativePersistenceBlockEdit> blocks)
    {
        if (file.BlockCount != (uint)blocks.Length)
        {
            throw new ArgumentException("World-block override block count must match the supplied block span.", nameof(blocks));
        }

        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceBlockEdit* blockPointer = blocks)
            {
                var result = s_writeWorldBlockOverrideFile(pathPointer, &file, blockPointer);
                if (result != 0)
                {
                    throw new IOException("Native world-block override write failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static int CountWorldBlockOverrideColumns(string path)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            uint columnCount = 0;
            var result = s_countWorldBlockOverrideColumns(pathPointer, &columnCount);
            if (result != 0 || columnCount > int.MaxValue)
            {
                return 0;
            }

            return checked((int)columnCount);
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void InitializeWorldBlockOverrides(
        string aggregatePath,
        string chunkDirectory,
        ReadOnlySpan<NativePersistenceBlockEdit> edits)
    {
        UpdateWorldBlockOverrides(
            aggregatePath,
            chunkDirectory,
            edits,
            s_initializeWorldBlockOverrides,
            "Native world-block override initialization failed.");
    }

    public static void SaveWorldBlockOverrides(
        string aggregatePath,
        string chunkDirectory,
        ReadOnlySpan<NativePersistenceBlockEdit> edits)
    {
        UpdateWorldBlockOverrides(
            aggregatePath,
            chunkDirectory,
            edits,
            s_saveWorldBlockOverrides,
            "Native world-block override save failed.");
    }

    public static IntPtr CreateWorldBlockSaveTracker()
    {
        var tracker = s_worldBlockSaveTrackerCreate();
        return tracker != IntPtr.Zero
            ? tracker
            : throw new InvalidOperationException("Native world-block save tracker allocation failed.");
    }

    public static void DestroyWorldBlockSaveTracker(IntPtr tracker)
    {
        if (tracker != IntPtr.Zero)
        {
            s_worldBlockSaveTrackerDestroy(tracker);
        }
    }

    public static void MarkWorldBlockSaveTrackerDirty(IntPtr tracker)
    {
        s_worldBlockSaveTrackerMarkDirty(RequireWorldBlockSaveTracker(tracker));
    }

    public static bool ShouldSaveWorldBlockOverrides(IntPtr tracker)
    {
        return s_worldBlockSaveTrackerShouldSave(RequireWorldBlockSaveTracker(tracker)) != 0;
    }

    public static void MarkWorldBlockSaveTrackerClean(IntPtr tracker)
    {
        s_worldBlockSaveTrackerMarkClean(RequireWorldBlockSaveTracker(tracker));
    }

    private static void UpdateWorldBlockOverrides(
        string aggregatePath,
        string chunkDirectory,
        ReadOnlySpan<NativePersistenceBlockEdit> edits,
        delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceBlockEdit*, uint, int> update,
        string failureMessage)
    {
        var aggregatePointer = Marshal.StringToCoTaskMemUTF8(aggregatePath);
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(chunkDirectory);
        try
        {
            fixed (NativePersistenceBlockEdit* editPointer = edits)
            {
                var result = update(aggregatePointer, directoryPointer, editPointer, checked((uint)edits.Length));
                if (result != 0)
                {
                    throw new IOException(failureMessage);
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(aggregatePointer);
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }

    private static IntPtr RequireWorldBlockSaveTracker(IntPtr tracker)
    {
        return tracker != IntPtr.Zero
            ? tracker
            : throw new ObjectDisposedException(nameof(WorldBlockPersistence));
    }
}
