using System.Text.Json.Serialization;
using Octaryn.Server.Simulation.Players;
using Octaryn.Shared.Time;

namespace Octaryn.Server.World.Chunks;

internal sealed class ChunkStreamSnapshotFile
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

    [JsonPropertyName("worldSeed")]
    public ulong WorldSeed { get; init; }

    [JsonPropertyName("worldTimeDayIndex")]
    public ulong WorldTimeDayIndex { get; init; }

    [JsonPropertyName("worldTimeSecondOfDay")]
    public uint WorldTimeSecondOfDay { get; init; }

    [JsonPropertyName("worldTimeTotalSeconds")]
    public double WorldTimeTotalSeconds { get; init; }

    [JsonPropertyName("worldTimeDayFraction")]
    public float WorldTimeDayFraction { get; init; }

    [JsonPropertyName("playerStateSource")]
    public string PlayerStateSource { get; init; } = "server_authority";

    [JsonPropertyName("playerX")]
    public float PlayerX { get; init; }

    [JsonPropertyName("playerY")]
    public float PlayerY { get; init; }

    [JsonPropertyName("playerZ")]
    public float PlayerZ { get; init; }

    [JsonPropertyName("playerPitch")]
    public float PlayerPitch { get; init; }

    [JsonPropertyName("playerYaw")]
    public float PlayerYaw { get; init; }

    [JsonPropertyName("playerVelocityX")]
    public float PlayerVelocityX { get; init; }

    [JsonPropertyName("playerVelocityY")]
    public float PlayerVelocityY { get; init; }

    [JsonPropertyName("playerVelocityZ")]
    public float PlayerVelocityZ { get; init; }

    [JsonPropertyName("playerControlMode")]
    public string ControlMode { get; init; } = "walk";

    [JsonPropertyName("playerOnGround")]
    public bool PlayerOnGround { get; init; }

    [JsonPropertyName("windowEpoch")]
    public ulong WindowEpoch { get; init; }

    [JsonPropertyName("windowLoadCount")]
    public int WindowLoadCount { get; init; }

    [JsonPropertyName("windowPreserveCount")]
    public int WindowPreserveCount { get; init; }

    [JsonPropertyName("windowUnloadCount")]
    public int WindowUnloadCount { get; init; }

    [JsonPropertyName("windowEvents")]
    public IReadOnlyList<ChunkWindowEventFile> WindowEvents { get; init; } = [];

    [JsonPropertyName("columns")]
    public IReadOnlyList<ChunkStreamColumnFile> Columns { get; init; } = [];

    [JsonPropertyName("blocks")]
    public IReadOnlyList<ChunkStreamBlockFile> Blocks { get; init; } = [];

    public static ChunkStreamSnapshotFile From(
        ulong epoch,
        ChunkColumnStream stream,
        WorldTimeSnapshot worldTime,
        PlayerState playerState)
    {
        return new ChunkStreamSnapshotFile
        {
            Epoch = epoch,
            CenterChunkX = stream.CenterChunkX,
            CenterChunkZ = stream.CenterChunkZ,
            Radius = stream.Radius,
            WorldSeed = 0,
            WorldTimeDayIndex = worldTime.DayIndex,
            WorldTimeSecondOfDay = worldTime.SecondOfDay,
            WorldTimeTotalSeconds = worldTime.TotalWorldSeconds,
            WorldTimeDayFraction = worldTime.DayFraction,
            PlayerX = playerState.X,
            PlayerY = playerState.Y,
            PlayerZ = playerState.Z,
            PlayerPitch = playerState.Pitch,
            PlayerYaw = playerState.Yaw,
            PlayerVelocityX = playerState.VelocityX,
            PlayerVelocityY = playerState.VelocityY,
            PlayerVelocityZ = playerState.VelocityZ,
            ControlMode = playerState.ControlMode == PlayerControlMode.Fly ? "fly" : "walk",
            PlayerOnGround = playerState.IsOnGround,
            WindowEpoch = stream.Window.Epoch,
            WindowLoadCount = stream.Window.LoadCount,
            WindowPreserveCount = stream.Window.PreserveCount,
            WindowUnloadCount = stream.Window.UnloadCount,
            WindowEvents = stream.Window.Events.Select(@event => new ChunkWindowEventFile
            {
                Kind = EventKindName(@event.Kind),
                ChunkX = @event.ChunkX,
                ChunkZ = @event.ChunkZ
            }).ToArray(),
            Columns = stream.Columns.Select(column => new ChunkStreamColumnFile
            {
                ChunkX = column.ChunkX,
                ChunkZ = column.ChunkZ,
                OriginX = column.OriginX,
                OriginZ = column.OriginZ,
                BlockOffset = column.BlockOffset,
                BlockCount = column.BlockCount
            }).ToArray(),
            Blocks = stream.Blocks.Select(block => new ChunkStreamBlockFile
            {
                X = block.X,
                Y = block.Y,
                Z = block.Z,
                Block = block.Block
            }).ToArray()
        };
    }

    private static string EventKindName(ChunkWindowEventKind kind)
    {
        return kind switch
        {
            ChunkWindowEventKind.Load => "load",
            ChunkWindowEventKind.Preserve => "preserve",
            ChunkWindowEventKind.Unload => "unload",
            _ => throw new ArgumentOutOfRangeException(nameof(kind), kind, null)
        };
    }
}

internal sealed class ChunkWindowEventFile
{
    [JsonPropertyName("kind")]
    public string Kind { get; init; } = "";

    [JsonPropertyName("chunkX")]
    public int ChunkX { get; init; }

    [JsonPropertyName("chunkZ")]
    public int ChunkZ { get; init; }
}

internal sealed class ChunkStreamColumnFile
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

internal sealed class ChunkStreamBlockFile
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
