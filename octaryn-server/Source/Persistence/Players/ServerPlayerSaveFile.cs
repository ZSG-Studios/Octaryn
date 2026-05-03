using System.Text.Json;
using System.Text.Json.Serialization;
using Octaryn.Shared.World;

namespace Octaryn.Server.Persistence.Players;

internal sealed class ServerPlayerSaveFile
{
    private const int CurrentVersion = 1;

    private static readonly JsonSerializerOptions s_options = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower
    };

    public int Version { get; init; } = CurrentVersion;

    public float X { get; init; }

    public float Y { get; init; }

    public float Z { get; init; }

    public float Pitch { get; init; }

    public float Yaw { get; init; }

    public ushort Block { get; init; }

    [JsonIgnore]
    public bool IsCurrent => Version == CurrentVersion;

    public static ServerPlayerSaveFile FromState(ServerPlayerSaveState state)
    {
        return new ServerPlayerSaveFile
        {
            X = state.X,
            Y = state.Y,
            Z = state.Z,
            Pitch = state.Pitch,
            Yaw = state.Yaw,
            Block = state.SelectedBlock.Value
        };
    }

    public ServerPlayerSaveState ToState()
    {
        return new ServerPlayerSaveState(X, Y, Z, Pitch, Yaw, new BlockId(Block));
    }

    public static bool TryLoad(string path, out ServerPlayerSaveState state)
    {
        state = default;
        if (!File.Exists(path))
        {
            return false;
        }

        var file = JsonSerializer.Deserialize<ServerPlayerSaveFile>(File.ReadAllText(path), s_options);
        if (file is null || !file.IsCurrent)
        {
            return false;
        }

        state = file.ToState();
        return true;
    }

    public static void Save(string path, ServerPlayerSaveState state)
    {
        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        var tempPath = $"{path}.tmp";
        File.WriteAllText(tempPath, JsonSerializer.Serialize(FromState(state), s_options));
        File.Move(tempPath, path, overwrite: true);
    }
}
