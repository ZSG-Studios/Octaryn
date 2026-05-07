using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

internal static partial class ServerPersistenceProbe
{
    private static void ValidatePlayerFileRoundTrip()
    {
        var root = ResetProbeDirectory("player-file");
        var path = Path.Combine(root, "player_1.json");
        var state = new PlayerSaveState(
            X: -200.5f,
            Y: 50.25f,
            Z: 3.5f,
            Pitch: -12.5f,
            Yaw: 91.25f,
            SelectedBlock: new BlockId(25));

        SavePlayerFile(path, state);
        Require(TryLoadPlayerFile(path, out var loaded), "player file load");
        Require(loaded == state, "player state round trip");

        var json = File.ReadAllText(path);
        Require(json.Contains("\"version\"", StringComparison.Ordinal), "player json version");
        Require(json.Contains("\"block\": 25", StringComparison.Ordinal), "player selected block stored as old block field");

        File.WriteAllText(path, json.Replace("\"version\": 1", "\"version\": 99", StringComparison.Ordinal));
        Require(!TryLoadPlayerFile(path, out _), "unknown player file version rejected");
    }

    private static void ValidatePlayerPersistenceRoot()
    {
        var root = ResetProbeDirectory("player-root");
        var previousRoot = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT");
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT", root);

        try
        {
            var persistence = PlayerPersistence.FromEnvironment();
            var path = persistence.PathFor(7);
            var expectedPath = Path.Combine(root, "player_7.json");
            Require(path == expectedPath, $"player path uses old file shape: {path} != {expectedPath}");
            Require(!persistence.TryLoad(7, out _), "missing player is absent");

            var state = new PlayerSaveState(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, new BlockId(6));
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

    private static bool TryLoadPlayerFile(string path, out PlayerSaveState state)
    {
        state = default;
        if (!NativeWorldPersistenceLibrary.TryReadPlayerFile(path, out var nativeState))
        {
            return false;
        }

        state = new PlayerSaveState(
            nativeState.X,
            nativeState.Y,
            nativeState.Z,
            nativeState.Pitch,
            nativeState.Yaw,
            new BlockId(nativeState.Block));
        return true;
    }

    private static void SavePlayerFile(string path, PlayerSaveState state)
    {
        NativeWorldPersistenceLibrary.WritePlayerFile(
            path,
            new NativePersistencePlayerState(
                state.X,
                state.Y,
                state.Z,
                state.Pitch,
                state.Yaw,
                state.SelectedBlock.Value));
    }
}
