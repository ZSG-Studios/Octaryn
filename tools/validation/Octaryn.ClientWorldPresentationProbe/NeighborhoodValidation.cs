using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.World;

internal static class NeighborhoodValidation
{
    public static void ValidateSnapshot()
    {
        var store = new ClientBlockPresentationStore();
        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 4, 1), new BlockId(11)), "snapshot center source block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(32, 4, 1), new BlockId(12)), "snapshot east source block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(-1, 4, 1), new BlockId(13)), "snapshot west source block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 4, 32), new BlockId(14)), "snapshot south source block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 4, -1), new BlockId(15)), "snapshot north source block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(64, 4, 1), new BlockId(16)), "snapshot outside source block");

        var snapshot = store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            ClientNeighborhoodBoundaryBlocks.StreamWindowEdge);
        ProbeAssertions.Require(snapshot.LocalBlock(1, 1, 1, 4, 1).Value == 11, "snapshot center local block");
        ProbeAssertions.Require(snapshot.NeighborhoodBlock(31, 4, 1, 1, 0, 0).Value == 12, "snapshot samples east neighbor");
        ProbeAssertions.Require(snapshot.NeighborhoodBlock(0, 4, 1, -1, 0, 0).Value == 13, "snapshot samples west neighbor");
        ProbeAssertions.Require(snapshot.NeighborhoodBlock(1, 4, 31, 0, 0, 1).Value == 14, "snapshot samples south neighbor");
        ProbeAssertions.Require(snapshot.NeighborhoodBlock(1, 4, 0, 0, 0, -1).Value == 15, "snapshot samples north neighbor");
        ProbeAssertions.Require(snapshot.NeighborhoodBlock(31, 4, 1, 33, 0, 0) == BlockId.Air, "snapshot outside neighborhood is air");
        ProbeAssertions.Require(snapshot.NeighborhoodBlock(1, 0, 1, 0, -1, 0) == BlockId.Air, "snapshot missing lower section is air");
        ProbeAssertions.Require(snapshot.NeighborhoodBlock(1, ClientChunkNeighborhoodSnapshot.Height - 1, 1, 0, 1, 0) == BlockId.Air, "snapshot missing upper section is air");

        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 4, 1), new BlockId(20)), "snapshot source mutates after capture");
        ProbeAssertions.Require(snapshot.LocalBlock(1, 1, 1, 4, 1).Value == 11, "snapshot is immutable after capture");

        var worldBottomStore = new ClientBlockPresentationStore();
        ProbeAssertions.Require(worldBottomStore.Apply(new BlockPosition(1, ChunkConstants.WorldMinY, 1), new BlockId(21)), "snapshot world bottom source block");
        var worldBottom = worldBottomStore.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, ChunkConstants.WorldMinY / ClientChunkNeighborhoodSnapshot.Height, 0),
            ClientNeighborhoodBoundaryBlocks.StreamWindowEdge);
        ProbeAssertions.Require(worldBottom.LocalBlock(1, 1, 1, 0, 1).Value == 21, "snapshot captures centered world bottom block");
        ProbeAssertions.Require(worldBottom.NeighborhoodBlock(1, 0, 1, 0, -1, 0).Value == 1, "snapshot below world uses boundary block");

        var worldTopStore = new ClientBlockPresentationStore();
        ProbeAssertions.Require(worldTopStore.Apply(new BlockPosition(1, ChunkConstants.WorldMaxYExclusive - 1, 1), new BlockId(22)), "snapshot world top source block");
        var worldTop = worldTopStore.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, (ChunkConstants.WorldMaxYExclusive - 1) / ClientChunkNeighborhoodSnapshot.Height, 0),
            ClientNeighborhoodBoundaryBlocks.StreamWindowEdge);
        ProbeAssertions.Require(worldTop.LocalBlock(1, 1, 1, ClientChunkNeighborhoodSnapshot.Height - 1, 1).Value == 22, "snapshot captures centered world top block");
        ProbeAssertions.Require(worldTop.NeighborhoodBlock(1, ClientChunkNeighborhoodSnapshot.Height - 1, 1, 0, 1, 0) == BlockId.Air, "snapshot above world is air");
    }

    public static void ValidateFaceVisibility()
    {
        var rules = new ClientBlockRenderRules();
        var store = new ClientBlockPresentationStore();
        ProbeAssertions.Require(store.Apply(new BlockPosition(31, 5, 1), new BlockId(1)), "east boundary center block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(32, 5, 1), new BlockId(5)), "east boundary opaque neighbor");
        ProbeAssertions.Require(store.Apply(new BlockPosition(0, 5, 1), new BlockId(1)), "west boundary center block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(-1, 5, 1), new BlockId(7)), "west boundary leaves neighbor");
        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 5, 31), new BlockId(1)), "south boundary center block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 5, 32), new BlockId(30)), "south boundary glass neighbor");
        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 5, 0), new BlockId(1)), "north boundary center block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 5, -1), new BlockId(9)), "north boundary sprite neighbor");
        ProbeAssertions.Require(store.Apply(new BlockPosition(4, ClientChunkNeighborhoodSnapshot.Height - 1, 4), new BlockId(1)), "top face center block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(5, 0, 5), new BlockId(1)), "bottom face center block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(5, -1, 5), new BlockId(1)), "lower section bottom occluder");

        var snapshot = store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            ClientNeighborhoodBoundaryBlocks.StreamWindowEdge);

        ProbeAssertions.Require(!ContainsFace(rules, snapshot, 31, 5, 1, Direction.PositiveX), "opaque east neighbor hides boundary face");
        ProbeAssertions.Require(ContainsFace(rules, snapshot, 0, 5, 1, Direction.NegativeX), "leaves west neighbor exposes boundary face");
        ProbeAssertions.Require(ContainsFace(rules, snapshot, 1, 5, 31, Direction.PositiveZ), "glass south neighbor exposes boundary face");
        ProbeAssertions.Require(ContainsFace(rules, snapshot, 1, 5, 0, Direction.NegativeZ), "sprite north neighbor exposes boundary face");
        ProbeAssertions.Require(ContainsFace(rules, snapshot, 4, ClientChunkNeighborhoodSnapshot.Height - 1, 4, Direction.PositiveY), "missing upper section exposes top face");
        ProbeAssertions.Require(!ContainsFace(rules, snapshot, 5, 0, 5, Direction.NegativeY), "lower section block hides bottom face");

        var airBelowStore = new ClientBlockPresentationStore();
        ProbeAssertions.Require(airBelowStore.Apply(new BlockPosition(5, 0, 5), new BlockId(1)), "air lower section center block");
        var airBelowSnapshot = airBelowStore.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            ClientNeighborhoodBoundaryBlocks.Air);
        ProbeAssertions.Require(ContainsFace(rules, airBelowSnapshot, 5, 0, 5, Direction.NegativeY), "missing lower section exposes bottom face");
    }

    private static bool ContainsFace(
        ClientBlockRenderRules rules,
        ClientChunkNeighborhoodSnapshot snapshot,
        int blockX,
        int blockY,
        int blockZ,
        Direction face)
    {
        return rules.VisibleCubeFaces(snapshot, blockX, blockY, blockZ).Contains(face);
    }
}
