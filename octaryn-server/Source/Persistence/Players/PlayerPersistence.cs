using Octaryn.Server.Persistence.WorldBlocks;

namespace Octaryn.Server.Persistence.Players;

internal sealed class PlayerPersistence(string rootPath)
{
    public static PlayerPersistence FromEnvironment()
    {
        return new PlayerPersistence(NativeWorldPersistenceLibrary.PlayerDirectoryPathFromEnvironment());
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
