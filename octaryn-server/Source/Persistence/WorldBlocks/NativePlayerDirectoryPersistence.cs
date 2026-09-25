using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static NativePersistencePlayerFileEntry[] ReadPlayerDirectory(string directory)
    {
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            uint playerCount = 0;
            if (s_readPlayerDirectoryCount(directoryPointer, &playerCount) != 0 ||
                playerCount > int.MaxValue)
            {
                return [];
            }

            var players = new NativePersistencePlayerFileEntry[checked((int)playerCount)];
            fixed (NativePersistencePlayerFileEntry* playerPointer = players)
            {
                uint written = 0;
                var result = s_readPlayerDirectoryFill(
                    directoryPointer,
                    playerPointer,
                    playerCount,
                    &written);
                return result == 0 && written == playerCount ? players : [];
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }

    public static bool TryReadPlayerDirectoryEntry(
        string directory,
        int playerId,
        out NativePersistencePlayerState state)
    {
        state = default;
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            fixed (NativePersistencePlayerState* statePointer = &state)
            {
                return s_readPlayerDirectoryEntry(directoryPointer, playerId, statePointer) == 0;
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }

    public static void WritePlayerDirectoryEntry(
        string directory,
        int playerId,
        NativePersistencePlayerState state)
    {
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
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
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }

    public static string PlayerDirectoryPath(string directory, int playerId)
    {
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            ulong requiredSize = 0;
            var countResult = s_playerDirectoryPath(directoryPointer, playerId, null, 0, &requiredSize);
            if (countResult != 0 || requiredSize == 0 || requiredSize > int.MaxValue)
            {
                throw new IOException("Native player directory path count failed.");
            }

            var bytes = new byte[checked((int)requiredSize)];
            fixed (byte* pathPointer = bytes)
            {
                ulong writtenSize = 0;
                var fillResult = s_playerDirectoryPath(
                    directoryPointer,
                    playerId,
                    pathPointer,
                    requiredSize,
                    &writtenSize);
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
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }
}
