using Octaryn.Server.Persistence.WorldBlocks;

internal static partial class ServerPersistenceProbe
{
    private static void ValidatePlayerFileRoundTrip()
    {
        var root = ResetProbeDirectory("player-file");
        var path = Path.Combine(root, "player_1.json");
        var state = PlayerState(-200.5f, 50.25f, 3.5f, -12.5f, 91.25f, 25);

        NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(root, 1, state);
        Require(NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(root, 1, out var loaded), "player file load");
        RequirePlayerState(loaded, state, "player state round trip");

        var json = File.ReadAllText(path);
        Require(json.Contains("\"version\"", StringComparison.Ordinal), "player json version");
        Require(json.Contains("\"block\": 25", StringComparison.Ordinal), "player selected block stored as old block field");

        File.WriteAllText(path, json.Replace("\"version\": 1", "\"version\": 99", StringComparison.Ordinal));
        Require(
            !NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(root, 1, out _),
            "unknown player file version rejected");
    }

    private static void ValidatePlayerPersistenceRoot()
    {
        var root = ResetProbeDirectory("player-root");
        var previousRoot = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT");
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT", root);

        try
        {
            var playerDirectory = NativeWorldPersistenceLibrary.PlayerDirectoryPathFromEnvironment();
            var path = NativeWorldPersistenceLibrary.PlayerDirectoryPath(playerDirectory, 7);
            var expectedPath = Path.Combine(root, "player_7.json");
            Require(path == expectedPath, $"player path uses old file shape: {path} != {expectedPath}");
            Require(
                !NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(playerDirectory, 7, out _),
                "missing player is absent");

            var state = PlayerState(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6);
            NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(playerDirectory, 7, state);
            Require(File.Exists(path), "player persistence writes file");
            Require(
                NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(playerDirectory, 7, out var loaded),
                "player persistence loads saved state");
            RequirePlayerState(loaded, state, "player persistence state matches");
        }
        finally
        {
            Environment.SetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT", previousRoot);
        }
    }

    private static NativePersistencePlayerState PlayerState(
        float x,
        float y,
        float z,
        float pitch,
        float yaw,
        ushort block)
    {
        return new NativePersistencePlayerState(x, y, z, pitch, yaw, block);
    }

    private static void RequirePlayerState(
        NativePersistencePlayerState actual,
        NativePersistencePlayerState expected,
        string message)
    {
        Require(actual.X == expected.X, $"{message} x");
        Require(actual.Y == expected.Y, $"{message} y");
        Require(actual.Z == expected.Z, $"{message} z");
        Require(actual.Pitch == expected.Pitch, $"{message} pitch");
        Require(actual.Yaw == expected.Yaw, $"{message} yaw");
        Require(actual.Block == expected.Block, $"{message} block");
    }
}
