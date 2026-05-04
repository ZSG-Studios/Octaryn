using System.Text.Json.Serialization;

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

    [JsonPropertyName("columns")]
    public IReadOnlyList<ServerChunkStreamColumnFile> Columns { get; init; } = [];

    [JsonPropertyName("blocks")]
    public IReadOnlyList<ServerChunkStreamBlockFile> Blocks { get; init; } = [];

    public static ServerChunkStreamSnapshotFile From(ulong epoch, ServerChunkColumnStream stream)
    {
        return new ServerChunkStreamSnapshotFile
        {
            Epoch = epoch,
            CenterChunkX = stream.CenterChunkX,
            CenterChunkZ = stream.CenterChunkZ,
            Radius = stream.Radius,
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
