using System.Globalization;
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
        if (!Directory.Exists(directory))
        {
            return [];
        }

        List<BlockEdit> edits = [];
        foreach (var path in Directory.EnumerateFiles(directory, "chunk_*.json"))
        {
            if (!TryParseChunkColumnPath(path, out var originX, out var originZ) ||
                !ChunkColumnOverrideFile.TryLoad(path, out var file) ||
                file.Cx != originX ||
                file.Cz != originZ)
            {
                continue;
            }

            edits.AddRange(file.ToEdits());
        }

        return edits
            .OrderBy(edit => edit.Position.X)
            .ThenBy(edit => edit.Position.Y)
            .ThenBy(edit => edit.Position.Z)
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
        Directory.CreateDirectory(directory);
        var plan = ChunkColumnPersistencePlan.Create(edits);
        var plannedOrigins = plan.Columns
            .Select(column => new ChunkColumnOrigin(column.OriginX, column.OriginZ))
            .ToHashSet();

        foreach (var path in Directory.EnumerateFiles(directory, "chunk_*.json"))
        {
            if (TryParseChunkColumnPath(path, out var originX, out var originZ) &&
                !plannedOrigins.Contains(new ChunkColumnOrigin(originX, originZ)))
            {
                File.Delete(path);
            }
        }

        foreach (var column in plan.Columns)
        {
            ChunkColumnOverrideFile.Save(
                PathFor(directory, column.OriginX, column.OriginZ),
                ChunkColumnOverrideFile.FromEdits(
                    column.OriginX,
                    column.OriginZ,
                    plan.EditsFor(column)));
        }
    }

    public static int CountColumns(IReadOnlyList<BlockEdit> edits)
    {
        return checked((int)ChunkColumnPersistencePlan.CountColumns(edits));
    }

    public static IReadOnlyList<ChunkColumnOverrideFile> BuildFiles(IReadOnlyList<BlockEdit> edits)
    {
        var plan = ChunkColumnPersistencePlan.Create(edits);
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
        return checked((int)NativeWorldPersistenceLibrary.ScanChunkOverrideDirectory(
            directory,
            string.Empty).BlockCount);
    }

    public static string PathFor(string directory, int originX, int originZ)
    {
        return Path.Combine(directory, $"chunk_{originX}_{originZ}.json");
    }

    private static bool TryParseChunkColumnPath(string path, out int originX, out int originZ)
    {
        originX = 0;
        originZ = 0;

        var name = Path.GetFileNameWithoutExtension(path);
        if (!name.StartsWith("chunk_", StringComparison.Ordinal))
        {
            return false;
        }

        var tokens = name["chunk_".Length..].Split('_', 2);
        return tokens.Length == 2 &&
            int.TryParse(tokens[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out originX) &&
            int.TryParse(tokens[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out originZ);
    }
    private readonly record struct ChunkColumnOrigin(int X, int Z);

    private sealed unsafe class ChunkColumnPersistencePlan
    {
        private ChunkColumnPersistencePlan(
            IReadOnlyList<NativePersistenceChunkColumn> columns,
            IReadOnlyList<BlockEdit> orderedEdits)
        {
            Columns = columns;
            _orderedEdits = orderedEdits;
        }

        public IReadOnlyList<NativePersistenceChunkColumn> Columns { get; }

        private readonly IReadOnlyList<BlockEdit> _orderedEdits;

        public static uint CountColumns(IReadOnlyList<BlockEdit> edits)
        {
            var nativeEdits = edits.Select(NativePersistenceBlockEdit.FromBlockEdit).ToArray();
            var counts = default(NativePersistencePlanCounts);
            fixed (NativePersistenceBlockEdit* editPointer = nativeEdits)
            {
                var countResult = NativeWorldPersistenceLibrary.PlanChunkColumnsCount(
                    editPointer,
                    (uint)nativeEdits.Length,
                    &counts);
                if (countResult != 0)
                {
                    throw new InvalidOperationException("Native world persistence chunk-column count failed.");
                }

                return counts.ColumnCount;
            }
        }

        public static ChunkColumnPersistencePlan Create(IReadOnlyList<BlockEdit> edits)
        {
            var nativeEdits = edits.Select(NativePersistenceBlockEdit.FromBlockEdit).ToArray();
            var counts = default(NativePersistencePlanCounts);
            fixed (NativePersistenceBlockEdit* editPointer = nativeEdits)
            {
                var countResult = NativeWorldPersistenceLibrary.PlanChunkColumnsCount(
                    editPointer,
                    (uint)nativeEdits.Length,
                    &counts);
                if (countResult != 0)
                {
                    throw new InvalidOperationException("Native world persistence chunk-column count failed.");
                }

                var columns = new NativePersistenceChunkColumn[counts.ColumnCount];
                var orderedNativeEdits = new NativePersistenceBlockEdit[counts.BlockCount];
                var written = default(NativePersistencePlanCounts);
                fixed (NativePersistenceChunkColumn* columnPointer = columns)
                fixed (NativePersistenceBlockEdit* orderedEditPointer = orderedNativeEdits)
                {
                    var fillResult = NativeWorldPersistenceLibrary.PlanChunkColumnsFill(
                        editPointer,
                        (uint)nativeEdits.Length,
                        columnPointer,
                        counts.ColumnCount,
                        orderedEditPointer,
                        counts.BlockCount,
                        &written);
                    if (fillResult != 0)
                    {
                        throw new InvalidOperationException("Native world persistence chunk-column fill failed.");
                    }
                }

                return new ChunkColumnPersistencePlan(
                    columns,
                    orderedNativeEdits.Select(edit => edit.ToBlockEdit()).ToArray());
            }
        }

        public IReadOnlyList<BlockEdit> EditsFor(NativePersistenceChunkColumn column)
        {
            return _orderedEdits
                .Skip(checked((int)column.BlockOffset))
                .Take(checked((int)column.BlockCount))
                .ToArray();
        }
    }
}
