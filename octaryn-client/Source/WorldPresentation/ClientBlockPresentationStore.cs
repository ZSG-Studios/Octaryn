using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal sealed class ClientBlockPresentationStore
{
    private readonly Dictionary<BlockPosition, BlockId> _blocks = new();
    private readonly Queue<ClientBlockPresentationUpdate> _updates = new();
    private readonly HashSet<ClientPresentationChunkKey> _dirtyChunks = new();

    public bool Apply(BlockPosition position, BlockId block)
    {
        var current = GetBlock(position);
        if (current == block)
        {
            return false;
        }

        if (block == BlockId.Air)
        {
            _blocks.Remove(position);
        }
        else
        {
            _blocks[position] = block;
        }

        var ownerChunk = ClientPresentationChunkKey.FromBlock(position);
        MarkDirtyChunks(position, ownerChunk);
        _updates.Enqueue(new ClientBlockPresentationUpdate(position, block, ownerChunk));
        return true;
    }

    public BlockId GetBlock(BlockPosition position)
    {
        return _blocks.GetValueOrDefault(position, BlockId.Air);
    }

    public int PendingUpdateCount => _updates.Count;

    public int DirtyChunkCount => _dirtyChunks.Count;

    public bool TryDequeueUpdate(out ClientBlockPresentationUpdate update)
    {
        return _updates.TryDequeue(out update);
    }

    public IReadOnlyList<ClientPresentationChunkKey> DrainDirtyChunks()
    {
        var chunks = _dirtyChunks.ToArray();
        _dirtyChunks.Clear();
        return chunks;
    }

    public bool TryPeekDirtyChunk(out ClientPresentationChunkKey chunk)
    {
        if (_dirtyChunks.Count == 0)
        {
            chunk = default;
            return false;
        }

        chunk = _dirtyChunks.First();
        return true;
    }

    public bool RemoveDirtyChunk(ClientPresentationChunkKey chunk)
    {
        return _dirtyChunks.Remove(chunk);
    }

    public ClientChunkNeighborhoodSnapshot CaptureNeighborhood(
        ClientPresentationChunkKey center,
        ClientNeighborhoodBoundaryBlocks boundaries)
    {
        return ClientChunkNeighborhoodSnapshot.Capture(center, boundaries, _blocks);
    }

    private void MarkDirtyChunks(BlockPosition position, ClientPresentationChunkKey ownerChunk)
    {
        _dirtyChunks.Add(ownerChunk);

        var localX = ClientPresentationChunkKey.LocalBlockCoordinate(position.X, ClientPresentationChunkKey.Width);
        if (localX == 0)
        {
            _dirtyChunks.Add(ownerChunk with { X = ownerChunk.X - 1 });
        }
        else if (localX == ClientPresentationChunkKey.Width - 1)
        {
            _dirtyChunks.Add(ownerChunk with { X = ownerChunk.X + 1 });
        }

        var localY = ClientPresentationChunkKey.LocalBlockCoordinate(position.Y, ClientPresentationChunkKey.Height);
        if (localY == 0)
        {
            AddDirtyVerticalNeighbor(ownerChunk, ownerChunk.Y - 1);
        }
        else if (localY == ClientPresentationChunkKey.Height - 1)
        {
            AddDirtyVerticalNeighbor(ownerChunk, ownerChunk.Y + 1);
        }

        var localZ = ClientPresentationChunkKey.LocalBlockCoordinate(position.Z, ClientPresentationChunkKey.Depth);
        if (localZ == 0)
        {
            _dirtyChunks.Add(ownerChunk with { Z = ownerChunk.Z - 1 });
        }
        else if (localZ == ClientPresentationChunkKey.Depth - 1)
        {
            _dirtyChunks.Add(ownerChunk with { Z = ownerChunk.Z + 1 });
        }
    }

    private void AddDirtyVerticalNeighbor(ClientPresentationChunkKey ownerChunk, int y)
    {
        if (ClientPresentationChunkKey.ContainsSectionY(y))
        {
            _dirtyChunks.Add(ownerChunk with { Y = y });
        }
    }
}
