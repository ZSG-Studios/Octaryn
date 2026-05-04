using System.Globalization;

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
        return Path.Combine(rootPath, $"player_{playerId.ToString(CultureInfo.InvariantCulture)}.json");
    }

    public bool TryLoad(int playerId, out PlayerSaveState state)
    {
        return PlayerSaveFile.TryLoad(PathFor(playerId), out state);
    }

    public void Save(int playerId, PlayerSaveState state)
    {
        PlayerSaveFile.Save(PathFor(playerId), state);
    }
}
