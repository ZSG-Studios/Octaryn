using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

internal static partial class ServerPersistenceProbe
{
    private const uint WorldTimeFileVersion = 1;

    private static bool TryLoadWorldTime(string path, out ProbeWorldTimeState blob)
    {
        blob = default;
        if (!NativeWorldPersistenceLibrary.TryReadWorldTimeFile(path, out var state) ||
            state.Version != WorldTimeFileVersion)
        {
            return false;
        }

        blob = new ProbeWorldTimeState(state.Version, state.DayIndex, state.SecondsOfDay);
        return true;
    }

    private static void SaveWorldTime(string path, ProbeWorldTimeState blob)
    {
        NativeWorldPersistenceLibrary.WriteWorldTimeFile(
            path,
            new NativePersistenceWorldTimeState(blob.Version, blob.DayIndex, blob.SecondsOfDay));
    }

    private static bool TryLoadWorldMetadata(string path, out NativePersistenceWorldMetadata metadata)
    {
        return NativeWorldPersistenceLibrary.TryReadWorldMetadataFile(path, out metadata);
    }

    private static void SaveWorldMetadata(string path, NativePersistenceWorldMetadata metadata)
    {
        NativeWorldPersistenceLibrary.WriteWorldMetadataFile(path, metadata);
    }
}

internal readonly record struct ProbeWorldTimeState(
    uint Version,
    ulong DayIndex,
    double SecondsOfDay);

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
