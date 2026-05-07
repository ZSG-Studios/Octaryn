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

        state = ToState(nativeState);
        return true;
    }

    public void Save(int playerId, PlayerSaveState state)
    {
        NativeWorldPersistenceLibrary.WritePlayerDirectoryEntry(
            rootPath,
            playerId,
            ToNativeState(state));
    }

    private static PlayerSaveState ToState(NativePersistencePlayerState state)
    {
        return new PlayerSaveState(state.X, state.Y, state.Z, state.Pitch, state.Yaw, new(state.Block));
    }

    private static NativePersistencePlayerState ToNativeState(PlayerSaveState state)
    {
        return new NativePersistencePlayerState(
            state.X,
            state.Y,
            state.Z,
            state.Pitch,
            state.Yaw,
            state.SelectedBlock.Value);
    }
}
