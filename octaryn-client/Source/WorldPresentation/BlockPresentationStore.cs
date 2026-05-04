using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal sealed class BlockPresentationStore
{
    private readonly Dictionary<BlockPosition, BlockId> _blocks = new();
    private readonly Dictionary<PresentationChunkKey, Dictionary<BlockPosition, BlockId>> _blocksByChunk = new();
    private readonly Queue<BlockPresentationUpdate> _updates = new();
    private readonly HashSet<PresentationChunkKey> _dirtyChunks = new();
    private readonly HashSet<PresentationChunkKey> _loadedChunks = new();

    public bool Apply(BlockPosition position, BlockId block)
    {
        var current = GetBlock(position);
        var ownerChunk = PresentationChunkKey.FromBlock(position);
        if (current == block)
        {
            return false;
        }

        if (block == BlockId.Air)
        {
            _blocks.Remove(position);
            if (_blocksByChunk.TryGetValue(ownerChunk, out var chunkBlocks))
            {
                chunkBlocks.Remove(position);
                if (chunkBlocks.Count == 0)
                {
                    _blocksByChunk.Remove(ownerChunk);
                }
            }
        }
        else
        {
            _blocks[position] = block;
            if (!_blocksByChunk.TryGetValue(ownerChunk, out var chunkBlocks))
            {
                chunkBlocks = [];
                _blocksByChunk[ownerChunk] = chunkBlocks;
            }

            chunkBlocks[position] = block;
            _loadedChunks.Add(ownerChunk);
        }

        MarkDirtyChunks(position, ownerChunk);
        _updates.Enqueue(new BlockPresentationUpdate(position, block, ownerChunk));
        return true;
    }

    public BlockId GetBlock(BlockPosition position)
    {
        return _blocks.GetValueOrDefault(position, BlockId.Air);
    }

    public int PendingUpdateCount => _updates.Count;

    public int DirtyChunkCount => _dirtyChunks.Count;

    public bool TryDequeueUpdate(out BlockPresentationUpdate update)
    {
        return _updates.TryDequeue(out update);
    }

    public IReadOnlyList<PresentationChunkKey> DrainDirtyChunks()
    {
        var chunks = _dirtyChunks.ToArray();
        _dirtyChunks.Clear();
        return chunks;
    }

    public bool TryPeekDirtyChunk(out PresentationChunkKey chunk)
    {
        if (_dirtyChunks.Count == 0)
        {
            chunk = default;
            return false;
        }

        chunk = _dirtyChunks.First();
        return true;
    }

    public bool RemoveDirtyChunk(PresentationChunkKey chunk)
    {
        return _dirtyChunks.Remove(chunk);
    }

    public ChunkNeighborhoodSnapshot CaptureNeighborhood(
        PresentationChunkKey center,
        NeighborhoodBoundaryBlocks boundaries)
    {
        return ChunkNeighborhoodSnapshot.Capture(center, boundaries, _loadedChunks, _blocksByChunk);
    }

    private void MarkDirtyChunks(BlockPosition position, PresentationChunkKey ownerChunk)
    {
        _dirtyChunks.Add(ownerChunk);

        var localX = PresentationChunkKey.LocalBlockCoordinate(position.X, PresentationChunkKey.Width);
        if (localX == 0)
        {
            _dirtyChunks.Add(ownerChunk with { X = ownerChunk.X - 1 });
        }
        else if (localX == PresentationChunkKey.Width - 1)
        {
            _dirtyChunks.Add(ownerChunk with { X = ownerChunk.X + 1 });
        }

        var localY = PresentationChunkKey.LocalBlockCoordinate(position.Y, PresentationChunkKey.Height);
        if (localY == 0)
        {
            AddDirtyVerticalNeighbor(ownerChunk, ownerChunk.Y - 1);
        }
        else if (localY == PresentationChunkKey.Height - 1)
        {
            AddDirtyVerticalNeighbor(ownerChunk, ownerChunk.Y + 1);
        }

        var localZ = PresentationChunkKey.LocalBlockCoordinate(position.Z, PresentationChunkKey.Depth);
        if (localZ == 0)
        {
            _dirtyChunks.Add(ownerChunk with { Z = ownerChunk.Z - 1 });
        }
        else if (localZ == PresentationChunkKey.Depth - 1)
        {
            _dirtyChunks.Add(ownerChunk with { Z = ownerChunk.Z + 1 });
        }
    }

    private void AddDirtyVerticalNeighbor(PresentationChunkKey ownerChunk, int y)
    {
        if (PresentationChunkKey.ContainsSectionY(y))
        {
            _dirtyChunks.Add(ownerChunk with { Y = y });
        }
    }
}
