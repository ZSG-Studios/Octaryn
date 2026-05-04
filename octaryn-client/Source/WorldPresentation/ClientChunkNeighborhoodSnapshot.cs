using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal sealed class ClientChunkNeighborhoodSnapshot
{
    public const int Width = ClientPresentationChunkKey.Width;
    public const int Height = ClientPresentationChunkKey.Height;
    public const int Depth = ClientPresentationChunkKey.Depth;
    private const int ChunkCount = 3;
    private const int BlocksPerChunk = Width * Height * Depth;

    private readonly ClientNeighborhoodBoundaryBlocks _boundaries;
    private readonly IReadOnlySet<ClientPresentationChunkKey> _loadedChunks;
    private readonly BlockId[] _blocks;

    private ClientChunkNeighborhoodSnapshot(
        ClientPresentationChunkKey center,
        ClientNeighborhoodBoundaryBlocks boundaries,
        IReadOnlySet<ClientPresentationChunkKey> loadedChunks,
        BlockId[] blocks)
    {
        Center = center;
        _boundaries = boundaries;
        _loadedChunks = loadedChunks;
        _blocks = blocks;
    }

    public ClientPresentationChunkKey Center { get; }

    public static ClientChunkNeighborhoodSnapshot Capture(
        ClientPresentationChunkKey center,
        ClientNeighborhoodBoundaryBlocks boundaries,
        IReadOnlySet<ClientPresentationChunkKey> loadedChunks,
        IReadOnlyDictionary<ClientPresentationChunkKey, Dictionary<BlockPosition, BlockId>> source)
    {
        var blocks = new BlockId[BlocksPerChunk * ChunkCount * ChunkCount * ChunkCount];
        for (var chunkX = 0; chunkX < ChunkCount; chunkX++)
        for (var chunkY = 0; chunkY < ChunkCount; chunkY++)
        for (var chunkZ = 0; chunkZ < ChunkCount; chunkZ++)
        {
            var chunk = new ClientPresentationChunkKey(
                center.X + chunkX - 1,
                center.Y + chunkY - 1,
                center.Z + chunkZ - 1);
            if (!source.TryGetValue(chunk, out var chunkBlocks))
            {
                continue;
            }

            foreach (var (position, block) in chunkBlocks)
            {
                var localX = ClientPresentationChunkKey.LocalBlockCoordinate(position.X, Width);
                var localY = ClientPresentationChunkKey.LocalBlockCoordinate(position.Y, Height);
                var localZ = ClientPresentationChunkKey.LocalBlockCoordinate(position.Z, Depth);
                blocks[SnapshotIndex(chunkX, chunkY, chunkZ, localX, localY, localZ)] = block;
            }
        }

        return new ClientChunkNeighborhoodSnapshot(center, boundaries, loadedChunks, blocks);
    }

    public BlockId LocalBlock(int chunkX, int chunkZ, int blockX, int blockY, int blockZ)
    {
        return LocalBlock(chunkX, 1, chunkZ, blockX, blockY, blockZ);
    }

    public BlockId LocalBlock(int chunkX, int chunkY, int chunkZ, int blockX, int blockY, int blockZ)
    {
        if (chunkX < 0 || chunkX > 2 || chunkY < 0 || chunkY > 2 || chunkZ < 0 || chunkZ > 2)
        {
            return BlockId.Air;
        }

        return WorldBlock(
            new ClientPresentationChunkKey(Center.X + chunkX - 1, Center.Y + chunkY - 1, Center.Z + chunkZ - 1),
            blockX,
            blockY,
            blockZ);
    }

    public BlockId NeighborhoodBlock(int blockX, int blockY, int blockZ, int dx, int dy, int dz)
    {
        var x = blockX + dx;
        var y = blockY + dy;
        var z = blockZ + dz;
        var chunkX = 1;
        var chunkY = 1;
        var chunkZ = 1;

        while (x < 0)
        {
            x += ClientPresentationChunkKey.Width;
            chunkX--;
        }

        while (x >= ClientPresentationChunkKey.Width)
        {
            x -= ClientPresentationChunkKey.Width;
            chunkX++;
        }

        while (y < 0)
        {
            y += ClientPresentationChunkKey.Height;
            chunkY--;
        }

        while (y >= ClientPresentationChunkKey.Height)
        {
            y -= ClientPresentationChunkKey.Height;
            chunkY++;
        }

        while (z < 0)
        {
            z += ClientPresentationChunkKey.Depth;
            chunkZ--;
        }

        while (z >= ClientPresentationChunkKey.Depth)
        {
            z -= ClientPresentationChunkKey.Depth;
            chunkZ++;
        }

        return LocalBlock(chunkX, chunkY, chunkZ, x, y, z);
    }

    private BlockId WorldBlock(ClientPresentationChunkKey chunk, int localX, int y, int localZ)
    {
        var worldY = chunk.Y * ClientPresentationChunkKey.Height + y;
        if (worldY < ChunkConstants.WorldMinY)
        {
            return _boundaries.BelowWorldBlock;
        }

        if (worldY >= ChunkConstants.WorldMaxYExclusive)
        {
            return BlockId.Air;
        }

        var chunkX = chunk.X - Center.X + 1;
        var chunkY = chunk.Y - Center.Y + 1;
        var chunkZ = chunk.Z - Center.Z + 1;
        if (chunkX is < 0 or > 2 || chunkZ is < 0 or > 2)
        {
            return _boundaries.MissingHorizontalChunkBlock;
        }

        if (chunkY is < 0 or > 2)
        {
            return BlockId.Air;
        }

        if (!_loadedChunks.Contains(chunk) && (chunk.X != Center.X || chunk.Z != Center.Z))
        {
            return _boundaries.MissingHorizontalChunkBlock;
        }

        return _blocks[SnapshotIndex(chunkX, chunkY, chunkZ, localX, y, localZ)];
    }

    private static int SnapshotIndex(int chunkX, int chunkY, int chunkZ, int localX, int y, int localZ)
    {
        return ((chunkX * ChunkCount + chunkY) * ChunkCount + chunkZ) * BlocksPerChunk +
            (localX * Height + y) * Depth +
            localZ;
    }
}
