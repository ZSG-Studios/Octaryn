using System.Text.Json.Serialization;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal sealed class ChunkColumnOverrideFile
{
    private const int CurrentVersion = 2;
    private const int LegacyLocalCoordinateVersion = 1;

    public int Version { get; init; } = CurrentVersion;

    public int Cx { get; init; }

    public int Cz { get; init; }

    public IReadOnlyList<ChunkColumnBlockOverrideRecord> Blocks { get; init; } = [];

    [JsonIgnore]
    public bool IsCurrent => Version == CurrentVersion;

    public static ChunkColumnOverrideFile FromEdits(int originX, int originZ, IEnumerable<BlockEdit> edits)
    {
        var records = edits
            .OrderBy(edit => edit.Position.Y)
            .ThenBy(edit => edit.Position.X)
            .ThenBy(edit => edit.Position.Z)
            .Select(edit => new ChunkColumnBlockOverrideRecord(
                edit.Position.X,
                edit.Position.Y,
                edit.Position.Z,
                edit.Block.Value))
            .ToArray();

        return new ChunkColumnOverrideFile
        {
            Cx = originX,
            Cz = originZ,
            Blocks = records
        };
    }

    public static ChunkColumnOverrideFile FromNativeEdits(int originX, int originZ, IEnumerable<NativePersistenceBlockEdit> edits)
    {
        var records = edits
            .OrderBy(edit => edit.Position.Y)
            .ThenBy(edit => edit.Position.X)
            .ThenBy(edit => edit.Position.Z)
            .Select(edit => new ChunkColumnBlockOverrideRecord(
                edit.Position.X,
                edit.Position.Y,
                edit.Position.Z,
                edit.Block))
            .ToArray();

        return new ChunkColumnOverrideFile
        {
            Cx = originX,
            Cz = originZ,
            Blocks = records
        };
    }

    public IEnumerable<BlockEdit> ToEdits()
    {
        foreach (var block in Blocks)
        {
            yield return new BlockEdit(
                new BlockPosition(block.Bx, block.By, block.Bz),
                new BlockId(block.Block));
        }
    }

    public static bool TryLoad(string path, out ChunkColumnOverrideFile file)
    {
        file = new ChunkColumnOverrideFile();
        if (!NativeWorldPersistenceLibrary.TryReadChunkOverrideFile(path, out var nativeFile, out var blocks))
        {
            return false;
        }

        file = new ChunkColumnOverrideFile
        {
            Version = checked((int)nativeFile.Version),
            Cx = nativeFile.Cx,
            Cz = nativeFile.Cz,
            Blocks = blocks.Select(block => block.ToBlock()).ToArray()
        };
        return true;
    }

    public static bool TryNormalize(ChunkColumnOverrideFile loaded, out ChunkColumnOverrideFile file)
    {
        return TryUpgrade(loaded, out file);
    }

    public static void Save(string path, ChunkColumnOverrideFile file)
    {
        var blocks = file.Blocks
            .Select(NativePersistenceChunkOverrideBlock.FromBlock)
            .ToArray();
        NativeWorldPersistenceLibrary.WriteChunkOverrideFile(
            path,
            new NativePersistenceChunkOverrideFile(
                checked((uint)file.Version),
                file.Cx,
                file.Cz,
                checked((uint)blocks.Length)),
            blocks);
    }

    private static bool TryUpgrade(ChunkColumnOverrideFile loaded, out ChunkColumnOverrideFile upgraded)
    {
        upgraded = loaded;
        if (loaded.IsCurrent)
        {
            return true;
        }

        if (loaded.Version != LegacyLocalCoordinateVersion)
        {
            return false;
        }

        var sawLocalOnly = false;
        var sawWorldOnly = false;
        foreach (var block in loaded.Blocks)
        {
            var alreadyWorldCoordinates =
                block.Bx >= loaded.Cx - 1 &&
                block.Bx <= loaded.Cx + BlockLimits.ChunkWidth &&
                block.Bz >= loaded.Cz - 1 &&
                block.Bz <= loaded.Cz + BlockLimits.ChunkDepth;
            var looksLikeLocalCoordinates =
                block.Bx >= -1 &&
                block.Bx <= BlockLimits.ChunkWidth &&
                block.Bz >= -1 &&
                block.Bz <= BlockLimits.ChunkDepth;

            if (!alreadyWorldCoordinates && !looksLikeLocalCoordinates)
            {
                return false;
            }

            sawLocalOnly |= looksLikeLocalCoordinates && !alreadyWorldCoordinates;
            sawWorldOnly |= alreadyWorldCoordinates && !looksLikeLocalCoordinates;
        }

        if (sawLocalOnly && sawWorldOnly)
        {
            return false;
        }

        if (!sawLocalOnly && !sawWorldOnly && loaded.Blocks.Count > 0)
        {
            return false;
        }

        var blocks = loaded.Blocks;
        if (sawLocalOnly)
        {
            blocks = loaded.Blocks
                .Select(block => block with
                {
                    Bx = block.Bx + loaded.Cx,
                    Bz = block.Bz + loaded.Cz
                })
                .ToArray();
        }

        upgraded = new ChunkColumnOverrideFile
        {
            Version = CurrentVersion,
            Cx = loaded.Cx,
            Cz = loaded.Cz,
            Blocks = blocks
        };
        return true;
    }
}
