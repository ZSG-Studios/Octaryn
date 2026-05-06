using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static void ImportSaveExportBundle(
        string worldRoot,
        NativePersistenceWorldTimeState? worldTime,
        ReadOnlySpan<NativePersistencePlayerFileEntry> players,
        ReadOnlySpan<NativePersistenceSaveImportChunk> chunks,
        ReadOnlySpan<NativePersistenceChunkOverrideBlock> blocks)
    {
        var rootPointer = Marshal.StringToCoTaskMemUTF8(worldRoot);
        try
        {
            var worldTimeState = worldTime.GetValueOrDefault();
            fixed (NativePersistencePlayerFileEntry* playerPointer = players)
            fixed (NativePersistenceSaveImportChunk* chunkPointer = chunks)
            fixed (NativePersistenceChunkOverrideBlock* blockPointer = blocks)
            {
                var worldTimePointer = worldTime.HasValue ? &worldTimeState : null;
                var result = s_importSaveExportBundle(
                    rootPointer,
                    worldTime.HasValue ? 1u : 0u,
                    worldTimePointer,
                    playerPointer,
                    checked((uint)players.Length),
                    chunkPointer,
                    checked((uint)chunks.Length),
                    blockPointer,
                    checked((uint)blocks.Length));
                if (result != 0)
                {
                    throw new IOException("Native save export bundle import failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(rootPointer);
        }
    }
}
