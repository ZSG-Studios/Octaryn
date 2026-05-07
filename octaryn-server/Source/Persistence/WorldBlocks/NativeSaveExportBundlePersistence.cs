using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    public static bool TryReadSaveExportBundle(
        string path,
        out NativePersistenceWorldTimeState? worldTime,
        out NativePersistencePlayerFileEntry[] players,
        out NativePersistenceSaveImportChunk[] chunks,
        out NativePersistenceChunkOverrideBlock[] blocks)
    {
        worldTime = null;
        players = [];
        chunks = [];
        blocks = [];
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            NativePersistenceSaveExportBundleCounts counts = default;
            if (s_readSaveExportBundleCount(pathPointer, &counts) != 0)
            {
                return false;
            }

            var loadedPlayers = new NativePersistencePlayerFileEntry[checked((int)counts.PlayerCount)];
            var loadedChunks = new NativePersistenceSaveImportChunk[checked((int)counts.ChunkCount)];
            var loadedBlocks = new NativePersistenceChunkOverrideBlock[checked((int)counts.BlockCount)];
            var loadedWorldTime = default(NativePersistenceWorldTimeState);
            fixed (NativePersistencePlayerFileEntry* playerPointer = loadedPlayers)
            fixed (NativePersistenceSaveImportChunk* chunkPointer = loadedChunks)
            fixed (NativePersistenceChunkOverrideBlock* blockPointer = loadedBlocks)
            {
                var written = default(NativePersistenceSaveExportBundleCounts);
                var result = s_readSaveExportBundleFill(
                    pathPointer,
                    &loadedWorldTime,
                    playerPointer,
                    counts.PlayerCount,
                    chunkPointer,
                    counts.ChunkCount,
                    blockPointer,
                    counts.BlockCount,
                    &written);
                if (result != 0 ||
                    written.HasWorldTime != counts.HasWorldTime ||
                    written.PlayerCount != counts.PlayerCount ||
                    written.ChunkCount != counts.ChunkCount ||
                    written.BlockCount != counts.BlockCount)
                {
                    return false;
                }
            }

            worldTime = counts.HasWorldTime != 0 ? loadedWorldTime : null;
            players = loadedPlayers;
            chunks = loadedChunks;
            blocks = loadedBlocks;
            return true;
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static void WriteSaveExportBundle(
        string path,
        uint bundleVersion,
        NativePersistenceWorldTimeState? worldTime,
        ReadOnlySpan<NativePersistencePlayerFileEntry> players,
        ReadOnlySpan<NativePersistenceSaveImportChunk> chunks,
        ReadOnlySpan<NativePersistenceChunkOverrideBlock> blocks)
    {
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var worldTimeState = worldTime.GetValueOrDefault();
            fixed (NativePersistencePlayerFileEntry* playerPointer = players)
            fixed (NativePersistenceSaveImportChunk* chunkPointer = chunks)
            fixed (NativePersistenceChunkOverrideBlock* blockPointer = blocks)
            {
                var worldTimePointer = worldTime.HasValue ? &worldTimeState : null;
                var result = s_writeSaveExportBundle(
                    pathPointer,
                    bundleVersion,
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
                    throw new IOException("Native save export bundle write failed.");
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

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
