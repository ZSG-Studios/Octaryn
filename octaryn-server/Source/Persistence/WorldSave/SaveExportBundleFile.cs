using System.Text.Json.Serialization;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.World.Time;

namespace Octaryn.Server.Persistence.WorldSave;

internal sealed class SaveExportBundleFile
{
    private const int CurrentVersion = 1;

    public int Version { get; init; } = CurrentVersion;

    public WorldTimeFile? WorldTime { get; init; }

    public IReadOnlyList<PlayerExportEntry> Players { get; init; } = [];

    public IReadOnlyList<ChunkColumnOverrideFile> Chunks { get; init; } = [];

    public static SaveExportBundleFile FromWorldRoot(string worldRoot)
    {
        WorldTimeFile? worldTime = null;
        if (NativeWorldPersistenceLibrary.TryReadWorldTimeFile(
                NativeWorldPersistenceLibrary.WorldTimePathForRoot(worldRoot),
                out var worldTimeState) &&
            worldTimeState.Version == WorldTimeBlob.CurrentVersion)
        {
            worldTime = new WorldTimeFile
            {
                Version = worldTimeState.Version,
                DayIndex = worldTimeState.DayIndex,
                SecondsOfDay = worldTimeState.SecondsOfDay
            };
        }

        return new SaveExportBundleFile
        {
            WorldTime = worldTime,
            Players = LoadPlayers(worldRoot),
            Chunks = LoadChunks(worldRoot)
        };
    }

    public static bool TryLoadGzip(string path, out SaveExportBundleFile bundle)
    {
        bundle = new SaveExportBundleFile();
        if (!NativeWorldPersistenceLibrary.TryReadSaveExportBundle(
                path,
                out var worldTime,
                out var players,
                out var chunks,
                out var blocks))
        {
            return false;
        }

        bundle = new SaveExportBundleFile
        {
            WorldTime = worldTime.HasValue ? WorldTimeFile.FromNativeState(worldTime.Value) : null,
            Players = players
                .Select(player => new PlayerExportEntry(player.PlayerId, PlayerExportData.FromNativeState(player.State)))
                .ToArray(),
            Chunks = ToChunkFiles(chunks, blocks)
        };
        return true;
    }

    public static void SaveGzip(string path, SaveExportBundleFile bundle)
    {
        NativeWorldPersistenceLibrary.WriteSaveExportBundle(
            path,
            checked((uint)bundle.Version),
            bundle.NativeWorldTime(validateVersion: true),
            bundle.NativePlayers(),
            bundle.NativeChunks(out var blocks),
            blocks);
    }

    public void WriteToWorldRoot(string worldRoot)
    {
        NativeWorldPersistenceLibrary.ImportSaveExportBundle(
            worldRoot,
            unchecked((uint)Version),
            NativeWorldTime(validateVersion: false),
            NativeImportPlayers(),
            NativeChunks(out var blocks),
            blocks);
    }

    private NativePersistenceWorldTimeState? NativeWorldTime(bool validateVersion)
    {
        if (WorldTime is not null)
        {
            if (validateVersion && WorldTime.Version != WorldTimeBlob.CurrentVersion)
            {
                throw new InvalidOperationException("Unsupported world time version.");
            }

            return new NativePersistenceWorldTimeState(
                checked((uint)WorldTime.Version),
                WorldTime.DayIndex,
                WorldTime.SecondsOfDay);
        }

        return null;
    }

    private NativePersistencePlayerFileEntry[] NativePlayers()
    {
        return Players.Select(player =>
        {
            if (!player.Data.IsCurrent)
            {
                throw new InvalidOperationException("Unsupported player save version.");
            }

            return new NativePersistencePlayerFileEntry(
                player.Id,
                player.Data.ToNativeState());
        }).ToArray();
    }

    private NativePersistenceSaveImportPlayer[] NativeImportPlayers()
    {
        return Players
            .Select(player => new NativePersistenceSaveImportPlayer(
                unchecked((uint)player.Data.Version),
                player.Id,
                player.Data.ToNativeState()))
            .ToArray();
    }

    private NativePersistenceSaveImportChunk[] NativeChunks(out NativePersistenceChunkOverrideBlock[] blocks)
    {
        var chunkPlans = new List<NativePersistenceSaveImportChunk>(Chunks.Count);
        var blockList = new List<NativePersistenceChunkOverrideBlock>();
        foreach (var chunk in Chunks)
        {
            var chunkBlocks = chunk.Blocks
                .Select(NativePersistenceChunkOverrideBlock.FromBlock)
                .ToArray();

            chunkPlans.Add(new NativePersistenceSaveImportChunk(
                checked((uint)chunk.Version),
                chunk.Cx,
                chunk.Cz,
                checked((uint)blockList.Count),
                checked((uint)chunkBlocks.Length)));
            blockList.AddRange(chunkBlocks);
        }

        blocks = blockList.ToArray();
        return chunkPlans.ToArray();
    }

    private static IReadOnlyList<PlayerExportEntry> LoadPlayers(string worldRoot)
    {
        return NativeWorldPersistenceLibrary.ReadPlayerDirectory(worldRoot)
            .Select(player => new PlayerExportEntry(player.PlayerId, PlayerExportData.FromNativeState(player.State)))
            .ToArray();
    }

    private static IReadOnlyList<ChunkColumnOverrideFile> LoadChunks(string worldRoot)
    {
        var plan = NativeWorldPersistenceLibrary.PlanWorldBlockExportColumns(
            NativeWorldPersistenceLibrary.WorldBlockOverridePathForRoot(worldRoot),
            worldRoot);
        return plan.Columns
            .Select(column => ChunkColumnOverrideFile.FromNativeEdits(
                column.OriginX,
                column.OriginZ,
                plan.NativeEditsFor(column)))
            .ToArray();
    }

    private static ChunkColumnOverrideFile[] ToChunkFiles(
        IReadOnlyList<NativePersistenceSaveImportChunk> chunks,
        IReadOnlyList<NativePersistenceChunkOverrideBlock> blocks)
    {
        return chunks
            .Select(chunk => new ChunkColumnOverrideFile
            {
                Version = checked((int)chunk.Version),
                Cx = chunk.Cx,
                Cz = chunk.Cz,
                Blocks = blocks
                    .Skip(checked((int)chunk.BlockOffset))
                    .Take(checked((int)chunk.BlockCount))
                    .Select(block => block.ToBlock())
                    .ToArray()
            })
            .ToArray();
    }
}

internal sealed record PlayerExportEntry(int Id, PlayerExportData Data);

internal sealed class PlayerExportData
{
    private const int CurrentVersion = 1;

    public int Version { get; init; } = CurrentVersion;

    public float X { get; init; }

    public float Y { get; init; }

    public float Z { get; init; }

    public float Pitch { get; init; }

    public float Yaw { get; init; }

    public ushort Block { get; init; }

    [JsonIgnore]
    public bool IsCurrent => Version == CurrentVersion;

    public static PlayerExportData FromNativeState(NativePersistencePlayerState state)
    {
        return new PlayerExportData
        {
            X = state.X,
            Y = state.Y,
            Z = state.Z,
            Pitch = state.Pitch,
            Yaw = state.Yaw,
            Block = state.Block
        };
    }

    public NativePersistencePlayerState ToNativeState()
    {
        return new NativePersistencePlayerState(X, Y, Z, Pitch, Yaw, Block);
    }
}

internal sealed class WorldTimeFile
{
    public uint Version { get; set; } = 1;

    public ulong DayIndex { get; set; }

    public double SecondsOfDay { get; set; }

    public static WorldTimeFile FromNativeState(NativePersistenceWorldTimeState state)
    {
        return new WorldTimeFile
        {
            Version = state.Version,
            DayIndex = state.DayIndex,
            SecondsOfDay = state.SecondsOfDay
        };
    }
}
