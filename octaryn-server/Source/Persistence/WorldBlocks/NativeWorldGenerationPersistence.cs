using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static void EnsureWorldGeneration(uint mode)
    {
        EnsureWorldGeneration(WorldRootPathFromEnvironment(), WorldBlockOverridePathFromEnvironment(),
            PlayerDirectoryPathFromEnvironment(), mode);
    }

    public static void EnsureWorldGenerationForRoot(string worldRoot)
    {
        EnsureWorldGeneration(worldRoot, WorldBlockOverridePathForRoot(worldRoot), worldRoot, 0u);
    }

    private static void EnsureWorldGeneration(string root, string blocksPath, string playersPath, uint mode)
    {
        var rootPointer = Marshal.StringToCoTaskMemUTF8(root);
        var blocksPointer = Marshal.StringToCoTaskMemUTF8(blocksPath);
        var playersPointer = Marshal.StringToCoTaskMemUTF8(playersPath);
        try
        {
            var result = s_ensureWorldGeneration(rootPointer, blocksPointer, playersPointer, mode);
            if (result != 0)
            {
                throw new IOException($"World generator identity check failed for '{root}' (code {result}). " +
                    "This save has missing, incompatible, or unreadable generator metadata. " +
                    "Choose a new world directory or explicitly migrate the old world before loading it. " +
                    "Existing save data was not changed.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(rootPointer);
            Marshal.FreeCoTaskMem(blocksPointer);
            Marshal.FreeCoTaskMem(playersPointer);
        }
    }
}
