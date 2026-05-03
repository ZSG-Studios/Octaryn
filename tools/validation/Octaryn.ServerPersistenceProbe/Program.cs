using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldSave;
using Octaryn.Server.Persistence.WorldTime;
using Octaryn.Server.World.Time;
using Octaryn.Shared.World;

return ServerPersistenceProbe.Run();

internal static class ServerPersistenceProbe
{
    public static int Run()
    {
        ValidatePlayerSaveFileRoundTrip();
        ValidatePlayerPersistenceRoot();
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

    private static void ValidateWorldSaveMetadata()
    {
        var root = ResetProbeDirectory("world-metadata");
        var emptyMetadata = ServerWorldSaveMetadataBuilder.Build(root);
        Require(!emptyMetadata.SaveExists, "empty metadata has no save");
        Require(emptyMetadata.PlayerCount == 0, "empty metadata player count");
        Require(emptyMetadata.BlockOverrideCount == 0, "empty metadata block count");

        WorldTimeStore.Save(Path.Combine(root, "world_time.json"), new WorldTimeBlob(1, 2, 30.5));

        var players = new ServerPlayerPersistence(root);
        players.Save(1, new ServerPlayerSaveState(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, new BlockId(6)));
        players.Save(2, new ServerPlayerSaveState(6.0f, 7.0f, 8.0f, 9.0f, 10.0f, new BlockId(11)));
        File.WriteAllText(Path.Combine(root, "player_invalid.json"), "{}");

        WorldBlockOverrideFile.Save(
            Path.Combine(root, "world_blocks.json"),
            WorldBlockOverrideFile.FromEdits([
                new BlockEdit(new BlockPosition(1, 2, 3), new BlockId(4)),
                new BlockEdit(new BlockPosition(5, 6, 7), new BlockId(8))
            ]));

        var metadata = ServerWorldSaveMetadataBuilder.Build(root);
        Require(metadata.SaveExists, "metadata detects save");
        Require(metadata.HasWorldTime, "metadata detects world time");
        Require(metadata.HasPlayerData, "metadata detects player data");
        Require(metadata.HasWorldData, "metadata detects world data");
        Require(metadata.PlayerCount == 2, "metadata counts valid player saves");
        Require(metadata.BlockOverrideCount == 2, "metadata counts block overrides");

        var metadataPath = Path.Combine(root, "world_meta.json");
        ServerWorldSaveMetadataFile.Save(metadataPath, metadata);
        Require(ServerWorldSaveMetadataFile.TryLoad(metadataPath, out var loaded), "metadata file load");
        Require(loaded == metadata, "metadata file round trip");

        var json = File.ReadAllText(metadataPath);
        Require(json.Contains("\"save_exists\": true", StringComparison.Ordinal), "metadata json save flag");
        Require(json.Contains("\"block_override_count\": 2", StringComparison.Ordinal), "metadata json block count");

        File.WriteAllText(metadataPath, json.Replace("\"version\": 1", "\"version\": 99", StringComparison.Ordinal));
        Require(!ServerWorldSaveMetadataFile.TryLoad(metadataPath, out _), "unknown metadata version rejected");
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
