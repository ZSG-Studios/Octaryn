using System.Text.Json.Serialization;

namespace Octaryn.Server.World.Chunks;

internal sealed class ServerWorldTimeIntentFile
{
    [JsonPropertyName("version")]
    public int Version { get; init; } = 1;

    [JsonPropertyName("speedIndex")]
    public int SpeedIndex { get; init; } = 2;

    [JsonPropertyName("speedMultiplier")]
    public double SpeedMultiplier { get; init; } = 1.0;

    [JsonIgnore]
    public bool IsSupported =>
        Version == 1 &&
        double.IsFinite(SpeedMultiplier) &&
        SpeedMultiplier is >= 0.0 and <= 24000.0;
}
