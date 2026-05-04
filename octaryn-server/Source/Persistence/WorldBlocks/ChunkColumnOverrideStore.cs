using System.Globalization;
using Octaryn.Server.World.Blocks;
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
        if (!Directory.Exists(directory))
        {
            return false;
        }

        var aggregateExists = File.Exists(aggregatePath);
        var aggregateTime = aggregateExists ? File.GetLastWriteTimeUtc(aggregatePath) : DateTime.MinValue;
        var hasChunkColumnFile = false;
        foreach (var path in Directory.EnumerateFiles(directory, "chunk_*.json"))
        {
            if (!TryParseChunkColumnPath(path, out var originX, out var originZ) ||
                !ChunkColumnOverrideFile.TryLoad(path, out var file) ||
                file.Cx != originX ||
                file.Cz != originZ)
            {
                continue;
            }

            hasChunkColumnFile = true;
            if (!aggregateExists || File.GetLastWriteTimeUtc(path) >= aggregateTime)
            {
                return true;
            }
        }

        return hasChunkColumnFile && !aggregateExists;
    }

    public static void SaveEdits(string directory, IReadOnlyList<BlockEdit> edits)
    {
        Directory.CreateDirectory(directory);
        var groupedEdits = edits
            .GroupBy(edit => ChunkColumnOriginFor(edit.Position))
            .ToDictionary(group => group.Key, group => (IReadOnlyList<BlockEdit>)group.ToArray());

        foreach (var path in Directory.EnumerateFiles(directory, "chunk_*.json"))
        {
            if (TryParseChunkColumnPath(path, out var originX, out var originZ) &&
                !groupedEdits.ContainsKey(new ChunkColumnOrigin(originX, originZ)))
            {
                File.Delete(path);
            }
        }

        foreach (var (origin, columnEdits) in groupedEdits.OrderBy(entry => entry.Key.X).ThenBy(entry => entry.Key.Z))
        {
            ChunkColumnOverrideFile.Save(
                PathFor(directory, origin.X, origin.Z),
                ChunkColumnOverrideFile.FromEdits(origin.X, origin.Z, columnEdits));
        }
    }

    public static int CountFiles(string directory)
    {
        if (!Directory.Exists(directory))
        {
            return 0;
        }

        HashSet<ChunkColumnOrigin> columns = [];
        foreach (var path in Directory.EnumerateFiles(directory, "chunk_*.json"))
        {
            if (TryParseChunkColumnPath(path, out var originX, out var originZ) &&
                ChunkColumnOverrideFile.TryLoad(path, out var file) &&
                file.Cx == originX &&
                file.Cz == originZ)
            {
                columns.Add(new ChunkColumnOrigin(originX, originZ));
            }
        }

        return columns.Count;
    }

    public static int CountBlocks(string directory)
    {
        if (!Directory.Exists(directory))
        {
            return 0;
        }

        var count = 0;
        foreach (var path in Directory.EnumerateFiles(directory, "chunk_*.json"))
        {
            if (TryParseChunkColumnPath(path, out var originX, out var originZ) &&
                ChunkColumnOverrideFile.TryLoad(path, out var file) &&
                file.Cx == originX &&
                file.Cz == originZ)
            {
                count += file.Blocks.Count;
            }
        }

        return count;
    }

    public static string PathFor(string directory, int originX, int originZ)
    {
        return Path.Combine(directory, $"chunk_{originX}_{originZ}.json");
    }

    private static ChunkColumnOrigin ChunkColumnOriginFor(BlockPosition position)
    {
        return new ChunkColumnOrigin(
            FloorDiv(position.X, BlockLimits.ChunkWidth) * BlockLimits.ChunkWidth,
            FloorDiv(position.Z, BlockLimits.ChunkDepth) * BlockLimits.ChunkDepth);
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

    private static int FloorDiv(int value, int divisor)
    {
        var quotient = value / divisor;
        var remainder = value % divisor;
        return remainder < 0 ? quotient - 1 : quotient;
    }

    private readonly record struct ChunkColumnOrigin(int X, int Z);
}
