using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static string WorldRootPathFromEnvironment()
    {
        var worldBlocksPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        var buildPreset = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        return WithEnvironmentPointers(
            worldBlocksPath,
            buildPreset,
            (worldBlocksPointer, buildPresetPointer) => ReadNativePath(
                (byte* path, ulong pathCapacity, ulong* requiredSize) =>
                    s_worldRootPathForEnvironment(
                        worldBlocksPointer,
                        buildPresetPointer,
                        path,
                        pathCapacity,
                        requiredSize),
                "Native world-root path lookup failed."));
    }

    public static string WorldBlockOverridePathFromEnvironment()
    {
        var worldBlocksPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        var buildPreset = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        return WithEnvironmentPointers(
            worldBlocksPath,
            buildPreset,
            (worldBlocksPointer, buildPresetPointer) => ReadNativePath(
                (byte* path, ulong pathCapacity, ulong* requiredSize) =>
                    s_worldBlockOverridePathForEnvironment(
                        worldBlocksPointer,
                        buildPresetPointer,
                        path,
                        pathCapacity,
                        requiredSize),
                "Native world-block override path lookup failed."));
    }

    public static string PlayerDirectoryPathFromEnvironment()
    {
        var playerRoot = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT");
        var worldBlocksPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        var buildPreset = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        return WithEnvironmentPointers(
            playerRoot,
            worldBlocksPath,
            buildPreset,
            (playerRootPointer, worldBlocksPointer, buildPresetPointer) => ReadNativePath(
                (byte* path, ulong pathCapacity, ulong* requiredSize) =>
                    s_playerDirectoryPathForEnvironment(
                        playerRootPointer,
                        worldBlocksPointer,
                        buildPresetPointer,
                        path,
                        pathCapacity,
                        requiredSize),
                "Native player-directory path lookup failed."));
    }

    public static string ChunkDirectoryForAggregatePath(string aggregatePath)
    {
        var aggregatePointer = Marshal.StringToCoTaskMemUTF8(aggregatePath);
        try
        {
            return ReadNativePath(
                (byte* path, ulong pathCapacity, ulong* requiredSize) =>
                    s_chunkDirectoryForAggregatePath(aggregatePointer, path, pathCapacity, requiredSize),
                "Native chunk-directory path lookup failed.");
        }
        finally
        {
            Marshal.FreeCoTaskMem(aggregatePointer);
        }
    }

    private static TResult WithEnvironmentPointers<TResult>(
        string? first,
        string? second,
        Func<IntPtr, IntPtr, TResult> read)
    {
        var firstPointer = EnvironmentStringToPointer(first);
        var secondPointer = EnvironmentStringToPointer(second);
        try
        {
            return read(firstPointer, secondPointer);
        }
        finally
        {
            Marshal.FreeCoTaskMem(firstPointer);
            Marshal.FreeCoTaskMem(secondPointer);
        }
    }

    private static TResult WithEnvironmentPointers<TResult>(
        string? first,
        string? second,
        string? third,
        Func<IntPtr, IntPtr, IntPtr, TResult> read)
    {
        var firstPointer = EnvironmentStringToPointer(first);
        var secondPointer = EnvironmentStringToPointer(second);
        var thirdPointer = EnvironmentStringToPointer(third);
        try
        {
            return read(firstPointer, secondPointer, thirdPointer);
        }
        finally
        {
            Marshal.FreeCoTaskMem(firstPointer);
            Marshal.FreeCoTaskMem(secondPointer);
            Marshal.FreeCoTaskMem(thirdPointer);
        }
    }

    private static IntPtr EnvironmentStringToPointer(string? value)
    {
        return string.IsNullOrWhiteSpace(value)
            ? IntPtr.Zero
            : Marshal.StringToCoTaskMemUTF8(value);
    }

    private static string ReadNativePath(
        PathFill readPath,
        string failureMessage)
    {
        ulong requiredSize = 0;
        var countResult = readPath(null, 0, &requiredSize);
        if (countResult != 0 || requiredSize == 0 || requiredSize > int.MaxValue)
        {
            throw new IOException(failureMessage);
        }

        var bytes = new byte[checked((int)requiredSize)];
        fixed (byte* pathPointer = bytes)
        {
            ulong writtenSize = 0;
            var fillResult = readPath(pathPointer, requiredSize, &writtenSize);
            if (fillResult != 0 || writtenSize != requiredSize)
            {
                throw new IOException(failureMessage);
            }

            return Marshal.PtrToStringUTF8((IntPtr)pathPointer) ??
                throw new IOException(failureMessage);
        }
    }

    private unsafe delegate int PathFill(byte* path, ulong pathCapacity, ulong* requiredSize);
}
