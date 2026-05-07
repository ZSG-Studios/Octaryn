using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldSave;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Time;
using Octaryn.Shared.World;

internal static partial class ServerPersistenceProbe
{
    private static bool TryLoadWorldTime(string path, out WorldTimeBlob blob)
    {
        blob = default;
        if (!NativeWorldPersistenceLibrary.TryReadWorldTimeFile(path, out var state) ||
            state.Version != WorldTimeBlob.CurrentVersion)
        {
            return false;
        }

        blob = new WorldTimeBlob(state.Version, state.DayIndex, state.SecondsOfDay);
        return true;
    }

    private static void SaveWorldTime(string path, WorldTimeBlob blob)
    {
        NativeWorldPersistenceLibrary.WriteWorldTimeFile(
            path,
            new NativePersistenceWorldTimeState(blob.Version, blob.DayIndex, blob.SecondsOfDay));
    }

    private static bool TryLoadWorldMetadata(string path, out WorldSaveMetadata metadata)
    {
        metadata = default;
        if (!NativeWorldPersistenceLibrary.TryReadWorldMetadataFile(path, out var nativeMetadata))
        {
            return false;
        }

        metadata = new WorldSaveMetadata(
            nativeMetadata.SaveExists != 0u,
            nativeMetadata.HasWorldTime != 0u,
            nativeMetadata.HasPlayerData != 0u,
            nativeMetadata.HasWorldData != 0u,
            nativeMetadata.PlayerCount,
            nativeMetadata.ChunkOverrideCount);
        return true;
    }

    private static void SaveWorldMetadata(string path, WorldSaveMetadata metadata)
    {
        NativeWorldPersistenceLibrary.WriteWorldMetadataFile(
            path,
            new NativePersistenceWorldMetadata(
                metadata.SaveExists ? 1u : 0u,
                metadata.HasWorldTime ? 1u : 0u,
                metadata.HasPlayerData ? 1u : 0u,
                metadata.HasWorldData ? 1u : 0u,
                metadata.PlayerCount,
                metadata.ChunkOverrideCount));
    }
}

internal static class ChunkColumnProbeFiles
{
    public static void SaveEdits(string directory, IReadOnlyList<BlockEdit> edits)
    {
        var plan = NativeWorldPersistenceLibrary.PlanChunkColumns(edits);
        NativeWorldPersistenceLibrary.PruneStaleChunkOverrideFiles(directory, plan.Columns);
        NativeWorldPersistenceLibrary.WriteChunkOverrideDirectory(directory, plan.Columns, plan.OrderedEdits);
    }

    public static IReadOnlyList<BlockEdit> LoadEdits(string directory)
    {
        return NativeWorldPersistenceLibrary.ReadChunkOverrideDirectory(directory)
            .Select(edit => edit.ToBlockEdit())
            .ToArray();
    }

    public static int CountFiles(string directory)
    {
        return checked((int)NativeWorldPersistenceLibrary.ScanChunkOverrideDirectory(
            directory,
            string.Empty).FileCount);
    }

    public static int CountBlocks(string directory)
    {
        return NativeWorldPersistenceLibrary.CountChunkOverrideDirectoryBlocks(directory);
    }

    public static string PathFor(string directory, int originX, int originZ)
    {
        return Path.Combine(directory, $"chunk_{originX}_{originZ}.json");
    }
}
