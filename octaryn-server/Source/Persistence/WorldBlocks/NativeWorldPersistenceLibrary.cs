using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe class NativeWorldPersistenceLibrary
{
    private const string LibraryName = "octaryn_server_world_persistence";

    public static readonly delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int> PlanChunkColumnsCount;
    public static readonly delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int> PlanChunkColumnsFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, int> s_writeGzipFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong*, int> s_readGzipFileCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, ulong*, int> s_readGzipFileFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int> s_readPlayerFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int> s_writePlayerFile;

    static NativeWorldPersistenceLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        PlanChunkColumnsCount = (delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_chunk_columns_count");
        PlanChunkColumnsFill = (delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_chunk_columns_fill");
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
