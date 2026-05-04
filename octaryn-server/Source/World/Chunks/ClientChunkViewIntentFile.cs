using System.Text.Json.Serialization;

namespace Octaryn.Server.World.Chunks;

internal sealed class ClientChunkViewIntentFile
{
    [JsonPropertyName("version")]
    public int Version { get; init; }

    [JsonPropertyName("epoch")]
    public ulong Epoch { get; init; }

    [JsonPropertyName("centerChunkX")]
    public int CenterChunkX { get; init; }

    [JsonPropertyName("centerChunkZ")]
    public int CenterChunkZ { get; init; }

    [JsonPropertyName("radius")]
    public uint Radius { get; init; }

    [JsonPropertyName("hasPreviousWindow")]
    public bool HasPreviousWindow { get; init; }

    [JsonPropertyName("previousCenterChunkX")]
    public int PreviousCenterChunkX { get; init; }

    [JsonPropertyName("previousCenterChunkZ")]
    public int PreviousCenterChunkZ { get; init; }

    [JsonPropertyName("previousRadius")]
    public uint PreviousRadius { get; init; }
}
