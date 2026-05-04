using System.Globalization;

namespace Octaryn.Server.Persistence.Players;

internal sealed class ServerPlayerPersistence(string rootPath)
{
    public static ServerPlayerPersistence FromEnvironment()
    {
        var explicitRoot = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SAVE_ROOT");
        if (!string.IsNullOrWhiteSpace(explicitRoot))
        {
            return new ServerPlayerPersistence(explicitRoot);
        }

        var worldBlocksPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        if (!string.IsNullOrWhiteSpace(worldBlocksPath))
        {
            var worldRoot = Path.GetDirectoryName(worldBlocksPath);
            if (!string.IsNullOrWhiteSpace(worldRoot))
            {
                return new ServerPlayerPersistence(worldRoot);
            }
        }

        var presetName = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        if (string.IsNullOrWhiteSpace(presetName))
        {
            presetName = "debug-linux";
        }

        return new ServerPlayerPersistence(Path.Combine("build", presetName, "server", "world"));
    }

    public string PathFor(int playerId)
    {
        return Path.Combine(rootPath, $"player_{playerId.ToString(CultureInfo.InvariantCulture)}.json");
    }

    public bool TryLoad(int playerId, out ServerPlayerSaveState state)
    {
        return ServerPlayerSaveFile.TryLoad(PathFor(playerId), out state);
    }

    public void Save(int playerId, ServerPlayerSaveState state)
    {
        ServerPlayerSaveFile.Save(PathFor(playerId), state);
    }
}
