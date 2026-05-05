using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe class NativeWorldPersistenceLibrary
{
    private const string LibraryName = "octaryn_server_world_persistence";

    public static readonly delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int> PlanChunkColumnsCount;
    public static readonly delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int> PlanChunkColumnsFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideFile*, int> s_readChunkOverrideFileCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideBlock*, uint, NativePersistenceChunkOverrideFile*, int> s_readChunkOverrideFileFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideFile*, NativePersistenceChunkOverrideBlock*, int> s_writeChunkOverrideFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, int> s_writeGzipFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong*, int> s_readGzipFileCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, ulong*, int> s_readGzipFileFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int> s_readPlayerFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int> s_writePlayerFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldTimeState*, int> s_readWorldTimeFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldTimeState*, int> s_writeWorldTimeFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldMetadata*, int> s_readWorldMetadataFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldMetadata*, int> s_writeWorldMetadataFile;

    static NativeWorldPersistenceLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        PlanChunkColumnsCount = (delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_chunk_columns_count");
        PlanChunkColumnsFill = (delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_chunk_columns_fill");
        s_readChunkOverrideFileCount = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideFile*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_chunk_override_file_count");
        s_readChunkOverrideFileFill = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideBlock*, uint, NativePersistenceChunkOverrideFile*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_chunk_override_file_fill");
        s_writeChunkOverrideFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideFile*, NativePersistenceChunkOverrideBlock*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_chunk_override_file");
        s_writeGzipFile = (delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_gzip_file");
        s_readGzipFileCount = (delegate* unmanaged[Cdecl]<IntPtr, ulong*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_gzip_file_count");
        s_readGzipFileFill = (delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, ulong*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_gzip_file_fill");
        s_readPlayerFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_player_file");
        s_writePlayerFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_player_file");
        s_readWorldTimeFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldTimeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_world_time_file");
        s_writeWorldTimeFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldTimeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_world_time_file");
        s_readWorldMetadataFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldMetadata*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_world_metadata_file");
        s_writeWorldMetadataFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldMetadata*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_world_metadata_file");
    }

    public static bool TryReadChunkOverrideFile(
        string path,
        out NativePersistenceChunkOverrideFile file,
        out NativePersistenceChunkOverrideBlock[] blocks)
    {
        file = default;
        blocks = [];
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceChunkOverrideFile* filePointer = &file)
            {
                if (s_readChunkOverrideFileCount(pathPointer, filePointer) != 0 ||
                    file.BlockCount > int.MaxValue)
                {
                    return false;
                }

                blocks = new NativePersistenceChunkOverrideBlock[checked((int)file.BlockCount)];
                fixed (NativePersistenceChunkOverrideBlock* blockPointer = blocks)
                {
                    var result = s_readChunkOverrideFileFill(
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

    public static void WriteChunkOverrideFile(
        string path,
        NativePersistenceChunkOverrideFile file,
        ReadOnlySpan<NativePersistenceChunkOverrideBlock> blocks)
    {
        if (file.BlockCount != (uint)blocks.Length)
        {
            throw new ArgumentException("Chunk-column override block count must match the supplied block span.", nameof(blocks));
        }

        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceChunkOverrideBlock* blockPointer = blocks)
            {
                var result = s_writeChunkOverrideFile(pathPointer, &file, blockPointer);
                if (result != 0)
                {
                    throw new IOException("Native chunk-column override write failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static bool TryReadGzipFile(string path, out byte[] payload)
    {
        payload = [];
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            ulong size = 0;
            if (s_readGzipFileCount(pathPointer, &size) != 0 || size > int.MaxValue)
            {
                return false;
            }

            payload = new byte[(int)size];
            fixed (byte* payloadPointer = payload)
            {
                ulong written = 0;
                return s_readGzipFileFill(pathPointer, payloadPointer, size, &written) == 0 &&
                    written == size;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WriteGzipFile(string path, ReadOnlySpan<byte> payload)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (byte* payloadPointer = payload)
            {
                var result = s_writeGzipFile(pathPointer, payloadPointer, (ulong)payload.Length);
                if (result != 0)
                {
                    throw new IOException("Native gzip save export write failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static bool TryReadPlayerFile(string path, out NativePersistencePlayerState state)
    {
        state = default;
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistencePlayerState* statePointer = &state)
            {
                return s_readPlayerFile(pathPointer, statePointer) == 0;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WritePlayerFile(string path, NativePersistencePlayerState state)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var result = s_writePlayerFile(pathPointer, &state);
            if (result != 0)
            {
                throw new IOException("Native player save write failed.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static bool TryReadWorldTimeFile(string path, out NativePersistenceWorldTimeState state)
    {
        state = default;
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceWorldTimeState* statePointer = &state)
            {
                return s_readWorldTimeFile(pathPointer, statePointer) == 0;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WriteWorldTimeFile(string path, NativePersistenceWorldTimeState state)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var result = s_writeWorldTimeFile(pathPointer, &state);
            if (result != 0)
            {
                throw new IOException("Native world time save write failed.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static bool TryReadWorldMetadataFile(string path, out NativePersistenceWorldMetadata metadata)
    {
        metadata = default;
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            fixed (NativePersistenceWorldMetadata* metadataPointer = &metadata)
            {
                return s_readWorldMetadataFile(pathPointer, metadataPointer) == 0;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WriteWorldMetadataFile(string path, NativePersistenceWorldMetadata metadata)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var result = s_writeWorldMetadataFile(pathPointer, &metadata);
            if (result != 0)
            {
                throw new IOException("Native world metadata save write failed.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeWorldPersistenceLibrary).Assembly.Location;
        if (!string.IsNullOrWhiteSpace(assemblyPath))
        {
            var assemblyLibraryPath = Path.Combine(Path.GetDirectoryName(assemblyPath) ?? string.Empty, fileName);
            if (File.Exists(assemblyLibraryPath))
            {
                return assemblyLibraryPath;
            }
        }

        var bundledPath = Path.Combine(AppContext.BaseDirectory, fileName);
        return File.Exists(bundledPath) ? bundledPath : LibraryName;
    }
}
