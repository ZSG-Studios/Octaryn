using System.Text.Json.Serialization;
using Octaryn.Shared.Time;

namespace Octaryn.Server.World.Chunks;

internal sealed class ServerChunkStreamSnapshotFile
{
    [JsonPropertyName("version")]
    public int Version { get; init; } = 1;

    [JsonPropertyName("epoch")]
    public ulong Epoch { get; init; }

    [JsonPropertyName("source")]
    public string Source { get; init; } = "server_process_chunk_stream";

    [JsonPropertyName("centerChunkX")]
    public int CenterChunkX { get; init; }

    [JsonPropertyName("centerChunkZ")]
    public int CenterChunkZ { get; init; }

    [JsonPropertyName("radius")]
    public uint Radius { get; init; }

    [JsonPropertyName("worldTimeDayIndex")]
    public ulong WorldTimeDayIndex { get; init; }

    [JsonPropertyName("worldTimeSecondOfDay")]
    public uint WorldTimeSecondOfDay { get; init; }

    [JsonPropertyName("worldTimeTotalSeconds")]
    public double WorldTimeTotalSeconds { get; init; }

    [JsonPropertyName("worldTimeDayFraction")]
    public float WorldTimeDayFraction { get; init; }

    [JsonPropertyName("windowEpoch")]
    public ulong WindowEpoch { get; init; }

    [JsonPropertyName("windowLoadCount")]
    public int WindowLoadCount { get; init; }

    [JsonPropertyName("windowPreserveCount")]
    public int WindowPreserveCount { get; init; }

    [JsonPropertyName("windowUnloadCount")]
    public int WindowUnloadCount { get; init; }

    [JsonPropertyName("windowEvents")]
    public IReadOnlyList<ServerChunkWindowEventFile> WindowEvents { get; init; } = [];

    [JsonPropertyName("columns")]
    public IReadOnlyList<ServerChunkStreamColumnFile> Columns { get; init; } = [];

    [JsonPropertyName("blocks")]
    public IReadOnlyList<ServerChunkStreamBlockFile> Blocks { get; init; } = [];

    public static ServerChunkStreamSnapshotFile From(
        ulong epoch,
        ServerChunkColumnStream stream,
        WorldTimeSnapshot worldTime)
    {
        return new ServerChunkStreamSnapshotFile
        {
            Epoch = epoch,
            CenterChunkX = stream.CenterChunkX,
            CenterChunkZ = stream.CenterChunkZ,
            Radius = stream.Radius,
            WorldTimeDayIndex = worldTime.DayIndex,
            WorldTimeSecondOfDay = worldTime.SecondOfDay,
            WorldTimeTotalSeconds = worldTime.TotalWorldSeconds,
            WorldTimeDayFraction = worldTime.DayFraction,
            WindowEpoch = stream.Window.Epoch,
            WindowLoadCount = stream.Window.LoadCount,
            WindowPreserveCount = stream.Window.PreserveCount,
            WindowUnloadCount = stream.Window.UnloadCount,
            WindowEvents = stream.Window.Events.Select(@event => new ServerChunkWindowEventFile
            {
                Kind = EventKindName(@event.Kind),
                ChunkX = @event.ChunkX,
                ChunkZ = @event.ChunkZ
            }).ToArray(),
            Columns = stream.Columns.Select(column => new ServerChunkStreamColumnFile
            {
                ChunkX = column.ChunkX,
                ChunkZ = column.ChunkZ,
                OriginX = column.OriginX,
                OriginZ = column.OriginZ,
                BlockOffset = column.BlockOffset,
                BlockCount = column.BlockCount
            }).ToArray(),
            Blocks = stream.Blocks.Select(block => new ServerChunkStreamBlockFile
            {
                X = block.X,
                Y = block.Y,
                Z = block.Z,
                Block = block.Block
            }).ToArray()
        };
    }

    private static string EventKindName(ServerChunkWindowEventKind kind)
    {
        return kind switch
        {
            ServerChunkWindowEventKind.Load => "load",
            ServerChunkWindowEventKind.Preserve => "preserve",
            ServerChunkWindowEventKind.Unload => "unload",
            _ => throw new ArgumentOutOfRangeException(nameof(kind), kind, null)
        };
    }
}

internal sealed class ServerChunkWindowEventFile
{
    [JsonPropertyName("kind")]
    public string Kind { get; init; } = "";

    [JsonPropertyName("chunkX")]
    public int ChunkX { get; init; }

    [JsonPropertyName("chunkZ")]
    public int ChunkZ { get; init; }
}

internal sealed class ServerChunkStreamColumnFile
{
    [JsonPropertyName("chunkX")]
    public int ChunkX { get; init; }

    [JsonPropertyName("chunkZ")]
    public int ChunkZ { get; init; }

    [JsonPropertyName("originX")]
    public int OriginX { get; init; }

    [JsonPropertyName("originZ")]
    public int OriginZ { get; init; }

    [JsonPropertyName("blockOffset")]
    public uint BlockOffset { get; init; }

    [JsonPropertyName("blockCount")]
    public uint BlockCount { get; init; }
}

internal sealed class ServerChunkStreamBlockFile
{
    [JsonPropertyName("x")]
    public int X { get; init; }

    [JsonPropertyName("y")]
    public int Y { get; init; }

    [JsonPropertyName("z")]
    public int Z { get; init; }

    [JsonPropertyName("block")]
    public ushort Block { get; init; }
}
