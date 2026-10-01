using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.World;

// World persistence native library: authoritative player save files. The
// static constructor resolves every export eagerly so a missing symbol fails
// at first use instead of mid-session.
internal static unsafe class NativeWorldPersistenceLibrary
{
    private const string LibraryName = "octaryn_server_world_persistence";

    private static readonly nint Library;
    private static readonly delegate* unmanaged[Cdecl]<byte*, int, NativePersistencePlayerState*, int> s_readPlayerDirectoryEntry;
    private static readonly delegate* unmanaged[Cdecl]<byte*, int, NativePersistencePlayerState*, int> s_writePlayerDirectoryEntry;
    private static readonly delegate* unmanaged[Cdecl]<byte*, byte*, byte*, byte*, ulong, ulong*, int> s_playerDirectoryPathForEnvironment;

    static NativeWorldPersistenceLibrary()
    {
        Library = NativeLibrary.Load(ResolveLibraryPath());
        s_readPlayerDirectoryEntry = (delegate* unmanaged[Cdecl]<byte*, int, NativePersistencePlayerState*, int>)Export(
            "octaryn_server_persistence_read_player_directory_entry");
        s_writePlayerDirectoryEntry = (delegate* unmanaged[Cdecl]<byte*, int, NativePersistencePlayerState*, int>)Export(
            "octaryn_server_persistence_write_player_directory_entry");
        s_playerDirectoryPathForEnvironment = (delegate* unmanaged[Cdecl]<byte*, byte*, byte*, byte*, ulong, ulong*, int>)Export(
            "octaryn_server_persistence_player_directory_path_for_environment");
    }

    public static bool TryReadPlayerDirectoryEntry(
        string directory,
        int playerId,
        out NativePersistencePlayerState state)
    {
        state = default;
        var directoryPointer = (byte*)Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            fixed (NativePersistencePlayerState* statePointer = &state)
            {
                return s_readPlayerDirectoryEntry(directoryPointer, playerId, statePointer) == 0;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)directoryPointer);
        }
    }

    public static void WritePlayerDirectoryEntry(
        string directory,
        int playerId,
        NativePersistencePlayerState state)
    {
        var directoryPointer = (byte*)Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            var result = s_writePlayerDirectoryEntry(directoryPointer, playerId, &state);
            if (result != 0)
            {
                throw new IOException("Native player directory entry write failed.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)directoryPointer);
        }
    }

    public static string PlayerDirectoryPathFromEnvironment()
    {
        var playerRoot = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT");
        var worldBlocksPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        var buildPreset = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        var playerRootPointer = (byte*)Marshal.StringToCoTaskMemUTF8(playerRoot ?? string.Empty);
        var worldBlocksPointer = (byte*)Marshal.StringToCoTaskMemUTF8(worldBlocksPath ?? string.Empty);
        var buildPresetPointer = (byte*)Marshal.StringToCoTaskMemUTF8(buildPreset ?? string.Empty);
        try
        {
            ulong requiredSize = 0;
            var countResult = s_playerDirectoryPathForEnvironment(
                playerRootPointer, worldBlocksPointer, buildPresetPointer, null, 0, &requiredSize);
            if (countResult != 0 || requiredSize == 0 || requiredSize > int.MaxValue)
            {
                throw new IOException("Native player directory path count failed.");
            }

            var bytes = new byte[checked((int)requiredSize)];
            fixed (byte* pathPointer = bytes)
            {
                ulong writtenSize = 0;
                var fillResult = s_playerDirectoryPathForEnvironment(
                    playerRootPointer, worldBlocksPointer, buildPresetPointer,
                    pathPointer, requiredSize, &writtenSize);
                if (fillResult != 0 || writtenSize != requiredSize)
                {
                    throw new IOException("Native player directory path fill failed.");
                }

                return Marshal.PtrToStringUTF8((IntPtr)pathPointer) ??
                    throw new IOException("Native player directory path decode failed.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)playerRootPointer);
            Marshal.FreeCoTaskMem((IntPtr)worldBlocksPointer);
            Marshal.FreeCoTaskMem((IntPtr)buildPresetPointer);
        }
    }

    private static nint Export(string name)
    {
        try
        {
            return NativeLibrary.GetExport(Library, name);
        }
        catch (EntryPointNotFoundException)
        {
            throw new EntryPointNotFoundException($"World persistence export missing: {name}");
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
