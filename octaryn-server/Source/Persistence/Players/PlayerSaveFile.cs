using System.Text.Json.Serialization;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.Players;

internal sealed class PlayerSaveFile
{
    private const int CurrentVersion = 1;

    public int Version { get; init; } = CurrentVersion;

    public float X { get; init; }

    public float Y { get; init; }

    public float Z { get; init; }

    public float Pitch { get; init; }

    public float Yaw { get; init; }

    public ushort Block { get; init; }

    [JsonIgnore]
    public bool IsCurrent => Version == CurrentVersion;

    public static PlayerSaveFile FromState(PlayerSaveState state)
    {
        return new PlayerSaveFile
        {
            X = state.X,
            Y = state.Y,
            Z = state.Z,
            Pitch = state.Pitch,
            Yaw = state.Yaw,
            Block = state.SelectedBlock.Value
        };
    }

    public static PlayerSaveFile FromNativeState(NativePersistencePlayerState state)
    {
        return new PlayerSaveFile
        {
            X = state.X,
            Y = state.Y,
            Z = state.Z,
            Pitch = state.Pitch,
            Yaw = state.Yaw,
            Block = state.Block
        };
    }

    public PlayerSaveState ToState()
    {
        return new PlayerSaveState(X, Y, Z, Pitch, Yaw, new BlockId(Block));
    }

    public NativePersistencePlayerState ToNativeState()
    {
        return new NativePersistencePlayerState(X, Y, Z, Pitch, Yaw, Block);
    }

    public static bool TryLoad(string path, out PlayerSaveState state)
    {
        state = default;
        if (!NativeWorldPersistenceLibrary.TryReadPlayerFile(path, out var nativeState))
        {
            return false;
        }

        state = FromNativeState(nativeState).ToState();
        return true;
    }

    public static void Save(string path, PlayerSaveState state)
    {
        NativeWorldPersistenceLibrary.WritePlayerFile(path, FromState(state).ToNativeState());
    }
}
