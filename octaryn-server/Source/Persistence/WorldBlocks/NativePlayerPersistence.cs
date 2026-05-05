using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
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
}
