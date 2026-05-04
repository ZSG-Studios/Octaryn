using System.Text.Json.Serialization;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Simulation.Players;

internal sealed class PlayerInputIntentFile
{
    [JsonPropertyName("version")]
    public int Version { get; init; } = 1;

    [JsonPropertyName("frameIndex")]
    public ulong FrameIndex { get; init; }

    [JsonPropertyName("deltaSeconds")]
    public double DeltaSeconds { get; init; } = 1.0 / 60.0;

    [JsonPropertyName("flags")]
    public uint Flags { get; init; }

    [JsonPropertyName("controller")]
    public uint Controller { get; init; }

    [JsonPropertyName("moveX")]
    public float MoveX { get; init; }

    [JsonPropertyName("moveY")]
    public float MoveY { get; init; }

    [JsonPropertyName("moveZ")]
    public float MoveZ { get; init; }

    [JsonPropertyName("cameraX")]
    public float CameraX { get; init; }

    [JsonPropertyName("cameraY")]
    public float CameraY { get; init; }

    [JsonPropertyName("cameraZ")]
    public float CameraZ { get; init; }

    [JsonPropertyName("cameraPitch")]
    public float CameraPitch { get; init; }

    [JsonPropertyName("cameraYaw")]
    public float CameraYaw { get; init; }

    [JsonPropertyName("relativeMouse")]
    public int RelativeMouse { get; init; }

    public bool IsSupported => Version == 1 &&
        FrameIndex > 0 &&
        double.IsFinite(DeltaSeconds) &&
        DeltaSeconds >= 0.0;

    public HostFrameSnapshot ToFrameSnapshot()
    {
        return new HostFrameSnapshot(
            new HostInputSnapshot(
                HostInputSnapshot.VersionValue,
                HostInputSnapshot.SizeValue,
                Flags,
                Controller,
                MoveX,
                MoveY,
                MoveZ,
                CameraX,
                CameraY,
                CameraZ,
                CameraPitch,
                CameraYaw,
                RelativeMouse),
            new HostFrameTimingSnapshot(
                HostFrameTimingSnapshot.VersionValue,
                HostFrameTimingSnapshot.SizeValue,
                FrameIndex,
                DeltaSeconds));
    }
}
