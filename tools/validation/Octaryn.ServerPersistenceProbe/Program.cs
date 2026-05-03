using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldSave;
using Octaryn.Server.Persistence.WorldTime;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Time;
using Octaryn.Shared.World;

return ServerPersistenceProbe.Run();

internal static class ServerPersistenceProbe
{
    public static int Run()
    {
        ValidatePlayerSaveFileRoundTrip();
        ValidatePlayerPersistenceRoot();
        ValidateChunkColumnOverrideFiles();
        ValidateWorldSaveMetadata();
        return 0;
    }

    private static void ValidatePlayerSaveFileRoundTrip()
    {
        var root = ResetProbeDirectory("player-file");
        var path = Path.Combine(root, "player_1.json");
        var state = new ServerPlayerSaveState(
            X: -200.5f,
            Y: 50.25f,
            Z: 3.5f,
            Pitch: -12.5f,
            Yaw: 91.25f,
            SelectedBlock: new BlockId(25));

        ServerPlayerSaveFile.Save(path, state);
        Require(ServerPlayerSaveFile.TryLoad(path, out var loaded), "player file load");
        Require(loaded == state, "player state round trip");

        var json = File.ReadAllText(path);
        Require(json.Contains("\"version\"", StringComparison.Ordinal), "player json version");
        Require(json.Contains("\"block\": 25", StringComparison.Ordinal), "player selected block stored as old block field");

        File.WriteAllText(path, json.Replace("\"version\": 1", "\"version\": 99", StringComparison.Ordinal));
        Require(!ServerPlayerSaveFile.TryLoad(path, out _), "unknown player file version rejected");
    }

    private static void ValidatePlayerPersistenceRoot()
    {
        var root = ResetProbeDirectory("player-root");
        var previousRoot = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT");
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT", root);

        try
        {
            var persistence = ServerPlayerPersistence.FromEnvironment();
            var path = persistence.PathFor(7);
            Require(path == Path.Combine(root, "player_7.json"), "player path uses old file shape");
            Require(!persistence.TryLoad(7, out _), "missing player is absent");

            var state = new ServerPlayerSaveState(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, new BlockId(6));
            persistence.Save(7, state);
            Require(File.Exists(path), "player persistence writes file");
            Require(persistence.TryLoad(7, out var loaded), "player persistence loads saved state");
            Require(loaded == state, "player persistence state matches");
        }
        finally
        {
            Environment.SetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT", previousRoot);
        }
    }

    private static void ValidateChunkColumnOverrideFiles()
    {
        var root = ResetProbeDirectory("chunk-columns");
        var edits = new[]
        {
            new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(5)),
            new BlockEdit(new BlockPosition(-1, 2, 31), new BlockId(6)),
            new BlockEdit(new BlockPosition(32, 3, 0), new BlockId(7))
        };

        ChunkColumnOverrideStore.SaveEdits(root, edits);
        var negativePath = ChunkColumnOverrideStore.PathFor(root, -32, 0);
        var originPath = ChunkColumnOverrideStore.PathFor(root, 0, 0);
        var positivePath = ChunkColumnOverrideStore.PathFor(root, 32, 0);
        Require(File.Exists(negativePath), "negative chunk column file written");
        Require(File.Exists(originPath), "origin chunk column file written");
        Require(File.Exists(positivePath), "positive chunk column file written");
        Require(ChunkColumnOverrideFile.TryLoad(originPath, out var originFile), "chunk column file load");
        Require(originFile.Version == 2, "chunk column file uses old current version");
        Require(originFile.Cx == 0 && originFile.Cz == 0, "chunk column origin stored");
        Require(originFile.Blocks.Count == 1, "chunk column blocks grouped");

        var json = File.ReadAllText(originPath);
        Require(json.Contains("\"version\": 2", StringComparison.Ordinal), "chunk column json version");
        Require(json.Contains("\"cx\": 0", StringComparison.Ordinal), "chunk column json cx");
        Require(json.Contains("\"bx\": 10", StringComparison.Ordinal), "chunk column json block x");

        var loadedEdits = ChunkColumnOverrideStore.LoadEdits(root);
        Require(loadedEdits.Count == 3, "chunk column load count");
        Require(loadedEdits[0].Position == new BlockPosition(-1, 2, 31), "chunk column load sorted negative");
        Require(ChunkColumnOverrideStore.CountFiles(root) == 3, "chunk column file count");
        Require(ChunkColumnOverrideStore.CountBlocks(root) == 3, "chunk column block count");

        File.WriteAllText(originPath, json.Replace("\"version\": 2", "\"version\": 99", StringComparison.Ordinal));
        Require(!ChunkColumnOverrideFile.TryLoad(originPath, out _), "unknown chunk column version rejected");

        ValidateLegacyChunkColumnOverrideMigration();

        ChunkColumnOverrideStore.SaveEdits(root, [edits[0]]);
        Require(!File.Exists(negativePath), "stale negative chunk column removed");
        Require(!File.Exists(positivePath), "stale positive chunk column removed");
        Require(File.Exists(originPath), "remaining chunk column rewritten");

        var worldBlocksPath = Path.Combine(root, "world_blocks.json");
        WorldBlockOverrideFile.Save(
            worldBlocksPath,
            WorldBlockOverrideFile.FromEdits([new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(99))]));
        var persistence = new ServerWorldBlockPersistence(worldBlocksPath);
        var loadedStore = new ServerBlockStore();
        persistence.Load(loadedStore);
        Require(loadedStore.GetBlock(new BlockPosition(10, 1, 2)).Value == 99, "newer aggregate file preferred over stale chunk columns");

        ChunkColumnOverrideStore.SaveEdits(root, [edits[0]]);
        loadedStore = new ServerBlockStore();
        persistence.Load(loadedStore);
        Require(loadedStore.GetBlock(new BlockPosition(10, 1, 2)).Value == 5, "current chunk columns preferred over aggregate file");

        loadedStore.SetBlock(new BlockEdit(new BlockPosition(64, 4, 0), new BlockId(8)));
        persistence.MarkDirty();
        persistence.SaveIfDirty(loadedStore);
        Require(WorldBlockOverrideFile.TryLoad(worldBlocksPath, out var aggregate), "aggregate override saved");
        Require(aggregate.Blocks.Count == 2, "aggregate override mirrors chunk columns");
        Require(File.Exists(ChunkColumnOverrideStore.PathFor(root, 64, 0)), "new chunk column saved on dirty flush");
    }

    private static void ValidateLegacyChunkColumnOverrideMigration()
    {
        var root = ResetProbeDirectory("chunk-column-legacy");
        var localPath = ChunkColumnOverrideStore.PathFor(root, 64, 0);
        ChunkColumnOverrideFile.Save(
            localPath,
            new ChunkColumnOverrideFile
            {
                Version = 1,
                Cx = 64,
                Cz = 0,
                Blocks = [new ChunkColumnBlockOverrideRecord(1, 2, 3, 10)]
            });
        Require(ChunkColumnOverrideFile.TryLoad(localPath, out var localFile), "legacy local chunk column migrates");
        var localEdit = localFile.ToEdits().Single();
        Require(localFile.Version == 2, "legacy local chunk column upgrades version");
        Require(localEdit.Position == new BlockPosition(65, 2, 3), "legacy local coordinates become world coordinates");

        var worldPath = ChunkColumnOverrideStore.PathFor(root, 64, 64);
        ChunkColumnOverrideFile.Save(
            worldPath,
            new ChunkColumnOverrideFile
            {
                Version = 1,
                Cx = 64,
                Cz = 64,
                Blocks = [new ChunkColumnBlockOverrideRecord(65, 2, 66, 11)]
            });
        Require(ChunkColumnOverrideFile.TryLoad(worldPath, out var worldFile), "legacy world chunk column migrates");
        var worldEdit = worldFile.ToEdits().Single();
        Require(worldEdit.Position == new BlockPosition(65, 2, 66), "legacy world coordinates stay unchanged");

        var ambiguousPath = ChunkColumnOverrideStore.PathFor(root, 0, 0);
        ChunkColumnOverrideFile.Save(
            ambiguousPath,
            new ChunkColumnOverrideFile
            {
                Version = 1,
                Cx = 0,
                Cz = 0,
                Blocks = [new ChunkColumnBlockOverrideRecord(1, 2, 3, 12)]
            });
        Require(!ChunkColumnOverrideFile.TryLoad(ambiguousPath, out _), "ambiguous legacy chunk column is rejected");
    }

    private static void ValidateWorldSaveMetadata()
    {
        var root = ResetProbeDirectory("world-metadata");
        var emptyMetadata = ServerWorldSaveMetadataBuilder.Build(root);
        Require(!emptyMetadata.SaveExists, "empty metadata has no save");
        Require(emptyMetadata.PlayerCount == 0, "empty metadata player count");
        Require(emptyMetadata.ChunkOverrideCount == 0, "empty metadata chunk count");

        WorldTimeStore.Save(Path.Combine(root, "world_time.json"), new WorldTimeBlob(1, 2, 30.5));

        var players = new ServerPlayerPersistence(root);
        players.Save(1, new ServerPlayerSaveState(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, new BlockId(6)));
        players.Save(2, new ServerPlayerSaveState(6.0f, 7.0f, 8.0f, 9.0f, 10.0f, new BlockId(11)));
        File.WriteAllText(Path.Combine(root, "player_invalid.json"), "{}");

        WorldBlockOverrideFile.Save(
            Path.Combine(root, "world_blocks.json"),
            WorldBlockOverrideFile.FromEdits([
                new BlockEdit(new BlockPosition(1, 2, 3), new BlockId(4)),
                new BlockEdit(new BlockPosition(5, 6, 7), new BlockId(8)),
                new BlockEdit(new BlockPosition(32, 6, 7), new BlockId(9))
            ]));

        var metadata = ServerWorldSaveMetadataBuilder.Build(root);
        Require(metadata.SaveExists, "metadata detects save");
        Require(metadata.HasWorldTime, "metadata detects world time");
        Require(metadata.HasPlayerData, "metadata detects player data");
        Require(metadata.HasWorldData, "metadata detects world data");
        Require(metadata.PlayerCount == 2, "metadata counts valid player saves");
        Require(metadata.ChunkOverrideCount == 2, "metadata counts unique aggregate chunk overrides");

        var metadataPath = Path.Combine(root, "world_meta.json");
        ServerWorldSaveMetadataFile.Save(metadataPath, metadata);
        Require(ServerWorldSaveMetadataFile.TryLoad(metadataPath, out var loaded), "metadata file load");
        Require(loaded == metadata, "metadata file round trip");

        var json = File.ReadAllText(metadataPath);
        Require(json.Contains("\"save_exists\": true", StringComparison.Ordinal), "metadata json save flag");
        Require(json.Contains("\"chunk_override_count\": 2", StringComparison.Ordinal), "metadata json chunk count");

        File.WriteAllText(metadataPath, json.Replace("\"version\": 1", "\"version\": 99", StringComparison.Ordinal));
        Require(!ServerWorldSaveMetadataFile.TryLoad(metadataPath, out _), "unknown metadata version rejected");

        var chunkOnlyRoot = ResetProbeDirectory("world-metadata-chunks");
        ChunkColumnOverrideStore.SaveEdits(
            chunkOnlyRoot,
            [
                new BlockEdit(new BlockPosition(0, 0, 0), new BlockId(1)),
                new BlockEdit(new BlockPosition(32, 0, 0), new BlockId(2))
            ]);
        var chunkOnlyMetadata = ServerWorldSaveMetadataBuilder.Build(chunkOnlyRoot);
        Require(chunkOnlyMetadata.SaveExists, "metadata detects chunk-only save");
        Require(chunkOnlyMetadata.HasWorldData, "metadata detects chunk-only world data");
        Require(chunkOnlyMetadata.ChunkOverrideCount == 2, "metadata counts chunk-only file overrides");
    }

    private static string ResetProbeDirectory(string name)
    {
        var root = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PERSISTENCE_PROBE_DIR");
        if (string.IsNullOrWhiteSpace(root))
        {
            var presetName = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
            if (string.IsNullOrWhiteSpace(presetName))
            {
                presetName = "debug-linux";
            }

            root = Path.Combine("build", presetName, "server", "validation", "server-persistence");
        }

        var directory = Path.Combine(root, name);
        if (Directory.Exists(directory))
        {
            Directory.Delete(directory, recursive: true);
        }

        Directory.CreateDirectory(directory);
        return directory;
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }
}
