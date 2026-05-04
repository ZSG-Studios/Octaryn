using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.World;

internal static class StoreValidation
{
    public static void Validate()
    {
        var store = new BlockPresentationStore();
        var origin = new BlockPosition(1, 5, 1);

        ProbeAssertions.Require(store.GetBlock(origin) == BlockId.Air, "missing block defaults to air");
        ProbeAssertions.Require(store.Apply(origin, new BlockId(7)), "new block updates presentation store");
        ProbeAssertions.Require(store.GetBlock(origin).Value == 7, "new block is visible in presentation store");
        ProbeAssertions.Require(store.PendingUpdateCount == 1, "new block enqueues presentation update");
        ProbeAssertions.Require(store.DirtyChunkCount == 1, "new block marks chunk dirty");

        ProbeAssertions.Require(!store.Apply(origin, new BlockId(7)), "unchanged block does not enqueue update");
        ProbeAssertions.Require(store.PendingUpdateCount == 1, "unchanged block keeps update count stable");

        ProbeAssertions.Require(store.TryDequeueUpdate(out var first), "first update dequeues");
        ProbeAssertions.Require(first.Position == origin, "first update position");
        ProbeAssertions.Require(first.Block.Value == 7, "first update block");
        ProbeAssertions.Require(first.Chunk == new PresentationChunkKey(0, 0, 0), "first update chunk");

        var negative = new BlockPosition(-1, 5, -33);
        ProbeAssertions.Require(store.Apply(negative, new BlockId(8)), "negative block updates presentation store");
        ProbeAssertions.Require(store.TryDequeueUpdate(out var second), "negative update dequeues");
        ProbeAssertions.Require(second.Chunk == new PresentationChunkKey(-1, 0, -2), "negative block uses floor chunk coordinates");

        var initialDirty = store.DrainDirtyChunks();
        ProbeAssertions.Require(initialDirty.Count == 4, "drain returns dirty chunks");
        ProbeAssertions.Require(initialDirty.Contains(first.Chunk), "drain includes first dirty chunk");
        ProbeAssertions.Require(initialDirty.Contains(second.Chunk), "drain includes second dirty chunk");
        ProbeAssertions.Require(initialDirty.Contains(new PresentationChunkKey(0, 0, -2)), "drain includes negative x border chunk");
        ProbeAssertions.Require(initialDirty.Contains(new PresentationChunkKey(-1, 0, -1)), "drain includes negative z border chunk");
        ProbeAssertions.Require(store.DirtyChunkCount == 0, "drain clears dirty chunks");

        var border = new BlockPosition(31, 5, 0);
        ProbeAssertions.Require(store.Apply(border, new BlockId(9)), "border block updates presentation store");
        var borderDirty = store.DrainDirtyChunks();
        ProbeAssertions.Require(borderDirty.Contains(new PresentationChunkKey(0, 0, 0)), "border edit marks owner chunk");
        ProbeAssertions.Require(borderDirty.Contains(new PresentationChunkKey(1, 0, 0)), "border edit marks east chunk");
        ProbeAssertions.Require(borderDirty.Contains(new PresentationChunkKey(0, 0, -1)), "border edit marks north chunk");
        ProbeAssertions.Require(borderDirty.Count == 3, "border edit marks only needed chunks");

        var verticalBorder = new BlockPosition(4, 0, 4);
        ProbeAssertions.Require(store.Apply(verticalBorder, new BlockId(10)), "vertical border block updates presentation store");
        var verticalDirty = store.DrainDirtyChunks();
        ProbeAssertions.Require(verticalDirty.Contains(new PresentationChunkKey(0, 0, 0)), "vertical border marks owner section");
        ProbeAssertions.Require(verticalDirty.Contains(new PresentationChunkKey(0, -1, 0)), "vertical border marks lower section");
        ProbeAssertions.Require(verticalDirty.Count == 2, "vertical border marks only needed sections");

        var bottomStore = new BlockPresentationStore();
        ProbeAssertions.Require(bottomStore.Apply(new BlockPosition(4, ChunkConstants.WorldMinY, 4), new BlockId(11)), "world bottom block updates presentation store");
        var bottomDirty = bottomStore.DrainDirtyChunks();
        ProbeAssertions.Require(bottomDirty.Contains(new PresentationChunkKey(0, PresentationChunkKey.MinSectionY, 0)), "world bottom marks owner section");
        ProbeAssertions.Require(bottomDirty.Count == 1, "world bottom does not mark below-world section");

        var topStore = new BlockPresentationStore();
        ProbeAssertions.Require(topStore.Apply(new BlockPosition(4, ChunkConstants.WorldMaxYExclusive - 1, 4), new BlockId(12)), "world top block updates presentation store");
        var topDirty = topStore.DrainDirtyChunks();
        ProbeAssertions.Require(topDirty.Contains(new PresentationChunkKey(0, PresentationChunkKey.MaxSectionYExclusive - 1, 0)), "world top marks owner section");
        ProbeAssertions.Require(topDirty.Count == 1, "world top does not mark above-world section");

        ProbeAssertions.Require(store.Apply(origin, BlockId.Air), "air removes presented block");
        ProbeAssertions.Require(store.GetBlock(origin) == BlockId.Air, "air removal visible");
    }
}
