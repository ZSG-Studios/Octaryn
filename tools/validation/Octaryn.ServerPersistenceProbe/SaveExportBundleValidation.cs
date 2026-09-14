using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldSave;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

internal static partial class ServerPersistenceProbe
{
    private static void ValidateServerSaveExportBundle()
    {
        var sourceRoot = ResetProbeDirectory("world-export-source");
        NativeWorldPersistenceLibrary.EnsureWorldGenerationForRoot(sourceRoot);
        SaveWorldTime(Path.Combine(sourceRoot, "world_time.json"), new ProbeWorldTimeState(1, 8, 42.25));

        var playerOne = PlayerState(-10.5f, 64.0f, 5.25f, 12.0f, 90.0f, 7);
        var playerTwo = PlayerState(16.0f, 70.0f, -3.0f, -2.0f, 180.0f, 11);
        NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(sourceRoot, 1, playerOne);
        NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(sourceRoot, 2, playerTwo);

        var edits = new[]
        {
            new BlockEdit(new BlockPosition(-1, 2, 31), new BlockId(6)),
            new BlockEdit(new BlockPosition(32, 3, 0), new BlockId(7))
        };
        WorldBlockOverrideProbeFile.Save(
            Path.Combine(sourceRoot, "world_blocks.json"),
            WorldBlockOverrideProbeFile.FromEdits(edits));

        var bundle = SaveExportBundleFile.FromWorldRoot(sourceRoot);
        Require(bundle.WorldTime is not null, "export bundle includes world time");
        Require(bundle.Players.Count == 2, "export bundle includes players");
        Require(bundle.Chunks.Count == 2, "export bundle groups aggregate block edits by chunk column");

        var exportPath = Path.Combine(sourceRoot, "server_save_export.json.gz");
        SaveExportBundleFile.SaveGzip(exportPath, bundle);
        Require(File.Exists(exportPath), "export bundle gzip written");
        Require(!File.Exists($"{exportPath}.tmp"), "export bundle gzip write replaces temp file");
        Require(SaveExportBundleFile.TryLoadGzip(exportPath, out var loadedBundle), "export bundle gzip loads");
        Require(bundle.GeneratorRevision == 3 && loadedBundle.GeneratorRevision == 3,
            "new terrain revision survives export codec");

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
        Require(NativeWorldPersistenceLibrary.WorldGenerationRevisionForRoot(targetRoot) == 3,
            "import retains vegetation generator revision");
        var oldRoot = ResetProbeDirectory("world-export-revision-two");
        var oldBundle = new SaveExportBundleFile { GeneratorRevision = 2 };
        oldBundle.WriteToWorldRoot(oldRoot);
        var revisionTwoPath = Path.Combine(sourceRoot, "revision-two.json.gz");
        SaveExportBundleFile.SaveGzip(revisionTwoPath, SaveExportBundleFile.FromWorldRoot(oldRoot));
        Require(SaveExportBundleFile.TryLoadGzip(revisionTwoPath, out var oldLoaded) && oldLoaded.GeneratorRevision == 2,
            "existing terrain revision survives export codec");
        var mismatchRejected = false;
        try { loadedBundle.WriteToWorldRoot(oldRoot); }
        catch (IOException) { mismatchRejected = true; }
        Require(mismatchRejected && NativeWorldPersistenceLibrary.WorldGenerationRevisionForRoot(oldRoot) == 2,
            "import rejects terrain rebase and preserves original identity");
        Require(!File.Exists(Path.Combine(oldRoot, "world_time.json")),
            "revision mismatch writes no imported world time");
        Require(TryLoadWorldTime(Path.Combine(targetRoot, "world_time.json"), out var loadedWorldTime), "import writes world time");
        Require(loadedWorldTime.DayIndex == 8 && loadedWorldTime.SecondsOfDay == 42.25, "imported world time matches");
        Require(
            NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(targetRoot, 1, out var loadedPlayerOne),
            "import writes first player");
        RequirePlayerState(loadedPlayerOne, playerOne, "imported first player matches");
        Require(
            NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(targetRoot, 2, out var loadedPlayerTwo),
            "import writes second player");
        RequirePlayerState(loadedPlayerTwo, playerTwo, "imported second player matches");
        Require(ChunkColumnProbeFiles.CountFiles(targetRoot) == 2, "import writes chunk column files");
        Require(ChunkColumnProbeFiles.CountBlocks(targetRoot) == 2, "import writes chunk column blocks");
        Require(WorldBlockOverrideProbeFile.TryLoad(Path.Combine(targetRoot, "world_blocks.json"), out var aggregate), "import mirrors aggregate world block file");
        Require(aggregate.Blocks.Count == 2, "import aggregate block count");

        var importedEdits = ChunkColumnProbeFiles.LoadEdits(targetRoot);
        Require(importedEdits.Count == 2, "imported chunk edits load");
        Require(importedEdits[0].Position == new BlockPosition(-1, 2, 31), "imported negative chunk edit matches");
        Require(importedEdits[1].Position == new BlockPosition(32, 3, 0), "imported positive chunk edit matches");

        var staleSourceRoot = ResetProbeDirectory("world-export-stale-source");
        NativeWorldPersistenceLibrary.EnsureWorldGenerationForRoot(staleSourceRoot);
        ChunkColumnProbeFiles.SaveEdits(
            staleSourceRoot,
            [new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(5))]);
        WorldBlockOverrideProbeFile.Save(
            Path.Combine(staleSourceRoot, "world_blocks.json"),
            WorldBlockOverrideProbeFile.FromEdits([new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(99))]));
        var staleBundleEdit = SaveExportBundleFile.FromWorldRoot(staleSourceRoot)
            .Chunks
            .SelectMany(chunk => ChunkColumnOverrideProbeFile.ToEdits(chunk))
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
        var rejectedWrite = false;
        try
        {
            SaveExportBundleFile.SaveGzip(unsupportedPath, new SaveExportBundleFile { Version = 99 });
        }
        catch (IOException) { rejectedWrite = true; }
        Require(rejectedWrite && !File.Exists(unsupportedPath), "unsupported export version rejected before writing");
        Require(NativeImportRejects(new SaveExportBundleFile { Version = 1 }),
            "unversioned legacy bundle rejected before writing destination");
        var unversionedRoot = ResetProbeDirectory("world-export-unversioned-source");
        var oldPath = Path.Combine(unversionedRoot, "player_1.json");
        File.WriteAllText(oldPath, "old saved position");
        var rejectedSource = false;
        try { SaveExportBundleFile.FromWorldRoot(unversionedRoot); }
        catch (IOException) { rejectedSource = true; }
        Require(rejectedSource && File.ReadAllText(oldPath) == "old saved position" &&
            !File.Exists(Path.Combine(unversionedRoot, "world_generation.json")),
            "unversioned export source rejected without modifying save");

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
