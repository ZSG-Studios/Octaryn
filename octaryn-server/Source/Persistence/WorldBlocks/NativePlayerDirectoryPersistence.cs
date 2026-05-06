using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static int CountPlayerDirectory(string directory)
    {
        var directoryPointer = Marshal.StringToCoTaskMemUTF8(directory);
        try
        {
            uint playerCount = 0;
            return s_readPlayerDirectoryCount(directoryPointer, &playerCount) == 0 &&
                playerCount <= int.MaxValue
                    ? checked((int)playerCount)
                    : 0;
        }
        finally
        {
            Marshal.FreeCoTaskMem(directoryPointer);
        }
    }

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
}
