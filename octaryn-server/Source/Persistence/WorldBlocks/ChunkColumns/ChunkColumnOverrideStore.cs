using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static class ChunkColumnOverrideStore
{
    public static string DirectoryForWorldBlocksPath(string worldBlocksPath)
    {
        return Path.GetDirectoryName(worldBlocksPath) ?? ".";
    }

    public static IReadOnlyList<BlockEdit> LoadEdits(string directory)
    {
        return NativeWorldPersistenceLibrary.ReadChunkOverrideDirectory(directory)
            .Select(edit => edit.ToBlockEdit())
            .ToArray();
    }

    public static bool HasCurrentFilesAtLeastAsNewAs(string directory, string aggregatePath)
    {
        return NativeWorldPersistenceLibrary.ScanChunkOverrideDirectory(
            directory,
            aggregatePath).CurrentFilesAtLeastAsNewAs != 0;
    }

    public static void SaveEdits(string directory, IReadOnlyList<BlockEdit> edits)
    {
        var plan = NativeWorldPersistenceLibrary.PlanChunkColumns(edits);
        NativeWorldPersistenceLibrary.PruneStaleChunkOverrideFiles(directory, plan.Columns);
        NativeWorldPersistenceLibrary.WriteChunkOverrideDirectory(
            directory,
            plan.Columns,
            plan.OrderedEdits);
    }

    public static IReadOnlyList<ChunkColumnOverrideFile> BuildFiles(IReadOnlyList<BlockEdit> edits)
    {
        var plan = NativeWorldPersistenceLibrary.PlanChunkColumns(edits);
        return plan.Columns
            .Select(column => ChunkColumnOverrideFile.FromEdits(
                column.OriginX,
                column.OriginZ,
                plan.EditsFor(column)))
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
