using Octaryn.Server.Persistence.WorldBlocks;

namespace Octaryn.Server.Persistence.Players;

internal sealed class PlayerPersistence(string rootPath)
{
    public static PlayerPersistence FromEnvironment()
    {
        var explicitRoot = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT");
        if (!string.IsNullOrWhiteSpace(explicitRoot))
        {
            return new PlayerPersistence(explicitRoot);
        }

        var worldBlocksPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        if (!string.IsNullOrWhiteSpace(worldBlocksPath))
        {
            var worldRoot = Path.GetDirectoryName(worldBlocksPath);
            if (!string.IsNullOrWhiteSpace(worldRoot))
            {
                return new PlayerPersistence(worldRoot);
            }
        }

        var presetName = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        if (string.IsNullOrWhiteSpace(presetName))
        {
            presetName = "debug-linux";
        }

        return new PlayerPersistence(Path.Combine("build", presetName, "server", "world"));
    }

    public string PathFor(int playerId)
    {
        return NativeWorldPersistenceLibrary.PlayerDirectoryPath(rootPath, playerId);
    }

    public bool TryLoad(int playerId, out PlayerSaveState state)
    {
        state = default;
        if (!NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(rootPath, playerId, out var nativeState))
        {
            return false;
        }

        state = PlayerSaveFile.FromNativeState(nativeState).ToState();
        return true;
    }

    public void Save(int playerId, PlayerSaveState state)
    {
        NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(
            rootPath,
            playerId,
            PlayerSaveFile.FromState(state).ToNativeState());
    }
}
