using System.Text.Json.Serialization;
using Octaryn.Shared.Host;

namespace Octaryn.Server.World.Blocks;

internal sealed class BlockInteractionIntentFile
{
    [JsonPropertyName("version")]
    public int Version { get; init; } = 1;

    [JsonPropertyName("frameIndex")]
    public ulong FrameIndex { get; init; }

    [JsonPropertyName("commands")]
    public IReadOnlyList<BlockInteractionCommandFile> Commands { get; init; } = [];

    public bool IsSupported => Version == 1 &&
        FrameIndex > 0 &&
        Commands.Count <= ClientBlockCommandQueue.MaxPendingCommands &&
        Commands.All(command => command.IsSupported);
}

internal sealed class BlockInteractionCommandFile
{
    [JsonPropertyName("requestId")]
    public ulong RequestId { get; init; }

    [JsonPropertyName("editX")]
    public int EditX { get; init; }

    [JsonPropertyName("editY")]
    public int EditY { get; init; }

    [JsonPropertyName("editZ")]
    public int EditZ { get; init; }

    [JsonPropertyName("block")]
    public ushort Block { get; init; }

    [JsonPropertyName("cameraX")]
    public float CameraX { get; init; }

    [JsonPropertyName("cameraY")]
    public float CameraY { get; init; }

    [JsonPropertyName("cameraZ")]
    public float CameraZ { get; init; }

    [JsonPropertyName("hitX")]
    public int HitX { get; init; }

    [JsonPropertyName("hitY")]
    public int HitY { get; init; }

    [JsonPropertyName("hitZ")]
    public int HitZ { get; init; }

    public bool IsSupported => RequestId != 0 &&
        float.IsFinite(CameraX) &&
        float.IsFinite(CameraY) &&
        float.IsFinite(CameraZ);

    public HostCommand ToHostCommand()
    {
        return new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            Flags = HostCommand.CriticalFlag | HostCommand.ClientInteractionFlag,
            RequestId = RequestId,
            A = EditX,
            B = EditY,
            C = EditZ,
            D = Block,
            X = CameraX,
            Y = CameraY,
            Z = CameraZ,
            X2 = HitX,
            Y2 = HitY,
            Z2 = HitZ
        };
    }
}
