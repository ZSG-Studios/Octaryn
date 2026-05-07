using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldSave;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Time;
using Octaryn.Shared.World;

internal static partial class ServerPersistenceProbe
{
    private static void ValidateServerSaveExportBundle()
    {
        var sourceRoot = ResetProbeDirectory("world-export-source");
        SaveWorldTime(Path.Combine(sourceRoot, "world_time.json"), new WorldTimeBlob(1, 8, 42.25));

        var players = new PlayerPersistence(sourceRoot);
        var playerOne = new PlayerSaveState(-10.5f, 64.0f, 5.25f, 12.0f, 90.0f, new BlockId(7));
        var playerTwo = new PlayerSaveState(16.0f, 70.0f, -3.0f, -2.0f, 180.0f, new BlockId(11));
        players.Save(1, playerOne);
        players.Save(2, playerTwo);

        var edits = new[]
        {
            new BlockEdit(new BlockPosition(-1, 2, 31), new BlockId(6)),
            new BlockEdit(new BlockPosition(32, 3, 0), new BlockId(7))
        };
        WorldBlockOverrideFile.Save(
            Path.Combine(sourceRoot, "world_blocks.json"),
            WorldBlockOverrideFile.FromEdits(edits));

        var bundle = SaveExportBundleFile.FromWorldRoot(sourceRoot);
        Require(bundle.WorldTime is not null, "export bundle includes world time");
        Require(bundle.Players.Count == 2, "export bundle includes players");
        Require(bundle.Chunks.Count == 2, "export bundle groups aggregate block edits by chunk column");

        var exportPath = Path.Combine(sourceRoot, "server_save_export.json.gz");
        SaveExportBundleFile.SaveGzip(exportPath, bundle);
        Require(File.Exists(exportPath), "export bundle gzip written");
        Require(!File.Exists($"{exportPath}.tmp"), "export bundle gzip write replaces temp file");
        Require(SaveExportBundleFile.TryLoadGzip(exportPath, out var loadedBundle), "export bundle gzip loads");

        var overwritePath = Path.Combine(sourceRoot, "server_save_export_overwrite.json.gz");
        SaveExportBundleFile.SaveGzip(overwritePath, bundle);
        SaveExportBundleFile.SaveGzip(
            overwritePath,
            new SaveExportBundleFile
            {
                WorldTime = new WorldTimeFile
                {
                    Version = 1,
                    DayIndex = 11,
                    SecondsOfDay = 12.5
                }
            });
        Require(SaveExportBundleFile.TryLoadGzip(overwritePath, out var overwrittenBundle), "export bundle gzip overwrites existing file");
        Require(overwrittenBundle.WorldTime is { DayIndex: 11, SecondsOfDay: 12.5 }, "overwritten export bundle content matches");

        var targetRoot = ResetProbeDirectory("world-export-target");
        loadedBundle.WriteToWorldRoot(targetRoot);
        Require(TryLoadWorldTime(Path.Combine(targetRoot, "world_time.json"), out var loadedWorldTime), "import writes world time");
        Require(loadedWorldTime.DayIndex == 8 && loadedWorldTime.SecondsOfDay == 42.25, "imported world time matches");
        Require(TryLoadPlayerFile(Path.Combine(targetRoot, "player_1.json"), out var loadedPlayerOne), "import writes first player");
        Require(loadedPlayerOne == playerOne, "imported first player matches");
        Require(TryLoadPlayerFile(Path.Combine(targetRoot, "player_2.json"), out var loadedPlayerTwo), "import writes second player");
        Require(loadedPlayerTwo == playerTwo, "imported second player matches");
        Require(ChunkColumnProbeFiles.CountFiles(targetRoot) == 2, "import writes chunk column files");
        Require(ChunkColumnProbeFiles.CountBlocks(targetRoot) == 2, "import writes chunk column blocks");
        Require(WorldBlockOverrideFile.TryLoad(Path.Combine(targetRoot, "world_blocks.json"), out var aggregate), "import mirrors aggregate world block file");
        Require(aggregate.Blocks.Count == 2, "import aggregate block count");

        var importedEdits = ChunkColumnProbeFiles.LoadEdits(targetRoot);
        Require(importedEdits.Count == 2, "imported chunk edits load");
        Require(importedEdits[0].Position == new BlockPosition(-1, 2, 31), "imported negative chunk edit matches");
        Require(importedEdits[1].Position == new BlockPosition(32, 3, 0), "imported positive chunk edit matches");

        var staleSourceRoot = ResetProbeDirectory("world-export-stale-source");
        ChunkColumnProbeFiles.SaveEdits(
            staleSourceRoot,
            [new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(5))]);
        WorldBlockOverrideFile.Save(
            Path.Combine(staleSourceRoot, "world_blocks.json"),
            WorldBlockOverrideFile.FromEdits([new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(99))]));
        var staleBundleEdit = SaveExportBundleFile.FromWorldRoot(staleSourceRoot)
            .Chunks
            .SelectMany(chunk => chunk.ToEdits())
            .Single();
        Require(staleBundleEdit.Block.Value == 99, "export uses active aggregate state over stale chunk columns");

        var legacyTargetRoot = ResetProbeDirectory("world-export-legacy-target");
        var legacyBundle = new SaveExportBundleFile
        {
            Chunks =
            [
                new ChunkColumnOverrideFile
                {
                    Version = 1,
                    Cx = 64,
                    Cz = 0,
                    Blocks = [new ChunkColumnBlockOverrideRecord(1, 2, 3, 10)]
                }
            ]
        };
        legacyBundle.WriteToWorldRoot(legacyTargetRoot);
        var legacyEdit = ChunkColumnProbeFiles.LoadEdits(legacyTargetRoot).Single();
        Require(legacyEdit.Position == new BlockPosition(65, 2, 3), "import normalizes legacy local chunk coordinates");

        Require(
            NativeImportRejects(
                new SaveExportBundleFile
                {
                    Version = 99
                }),
            "native import rejects unsupported bundle version");
        Require(
            NativeImportRejects(
                new SaveExportBundleFile
                {
                    WorldTime = new WorldTimeFile
                    {
                        Version = 99,
                        DayIndex = 1,
                        SecondsOfDay = 2
                    }
                }),
            "native import rejects unsupported world time version");
        Require(
            NativeImportRejects(
                new SaveExportBundleFile
                {
                    WorldTime = new WorldTimeFile
                    {
                        Version = 1,
                        DayIndex = 3,
                        SecondsOfDay = 4
                    },
                    Players =
                    [
                        new PlayerExportEntry(
                            4,
                            new PlayerExportData
                            {
                                Version = 99,
                                Y = 64,
                                Block = 1
                            })
                    ]
                }),
            "native import rejects unsupported player version");
        Require(
            NativeImportRejects(
                new SaveExportBundleFile
                {
                    WorldTime = new WorldTimeFile
                    {
                        Version = 1,
                        DayIndex = 5,
                        SecondsOfDay = 6
                    },
                    Players =
                    [
                        new PlayerExportEntry(
                            4,
                            new PlayerExportData
                            {
                                Y = 64,
                                Block = 1
                            })
                    ],
                    Chunks =
                    [
                        new ChunkColumnOverrideFile
                        {
                            Version = 99,
                            Blocks = [new ChunkColumnBlockOverrideRecord(1, 2, 3, 4)]
                        }
                    ]
                }),
            "native import rejects unsupported chunk version");

        var unsupportedPath = Path.Combine(sourceRoot, "unsupported_server_save_export.json.gz");
        SaveExportBundleFile.SaveGzip(
            unsupportedPath,
            new SaveExportBundleFile
            {
                Version = 99
            });
        Require(!SaveExportBundleFile.TryLoadGzip(unsupportedPath, out _), "unsupported export bundle version rejected");

        var corruptPath = Path.Combine(sourceRoot, "corrupt_server_save_export.json.gz");
        File.WriteAllText(corruptPath, "not a gzip save export");
        Require(!SaveExportBundleFile.TryLoadGzip(corruptPath, out _), "corrupt export bundle gzip rejected");
    }

    private static bool NativeImportRejects(SaveExportBundleFile bundle)
    {
        var root = ResetProbeDirectory("world-export-rejected-target");
        try
        {
            bundle.WriteToWorldRoot(root);
            return false;
        }
        catch (IOException)
        {
            return !Directory.EnumerateFileSystemEntries(root).Any();
        }
    }
}
