using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.World;

internal static class MeshPlanningValidation
{
    public static void ValidateChunkMeshPlanner()
    {
        var rules = new ClientBlockRenderRules();
        var planner = new ClientChunkMeshPlanner(rules);
        var store = new ClientBlockPresentationStore();

        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 4, 1), new BlockId(1)), "planner opaque cube block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(2, 4, 1), new BlockId(5)), "planner opaque cube occluder");
        ProbeAssertions.Require(store.Apply(new BlockPosition(4, 4, 4), new BlockId(30)), "planner glass block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(5, 4, 4), new BlockId(30)), "planner glass neighbor");
        ProbeAssertions.Require(store.Apply(new BlockPosition(8, 4, 8), new BlockId(9)), "planner cross sprite block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(10, 4, 10), new BlockId(22)), "planner solid-base sprite block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(12, 4, 12), new BlockId(14)), "planner water block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(14, 4, 14), new BlockId(31)), "planner lava block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(16, 4, 16), new BlockId(8)), "planner cloud block");

        var snapshot = store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            ClientNeighborhoodBoundaryBlocks.Air);
        var plan = planner.Build(snapshot);
        var repeatedPlan = planner.Build(snapshot);

        ProbeAssertions.Require(plan.CubeFaces.Count(face => face.Block.Value == 1) == 5, "planner hides cube face against opaque neighbor");
        ProbeAssertions.Require(plan.CubeFaces.First(face => face.Block.Value == 1).Direction == Direction.PositiveZ, "planner preserves cube face order");
        ProbeAssertions.Require(plan.CubeFaces.Any(face => face.Block.Value == 1 && face.Direction == Direction.NegativeX), "planner keeps visible cube face");
        ProbeAssertions.Require(plan.CubeFaces.Any(face => face.Block.Value == 1 && face.Kind == ClientBlockRenderKind.OpaqueCube), "planner tags opaque cube face");
        ProbeAssertions.Require(!plan.CubeFaces.Any(face => face.Block.Value == 1 && face.Direction == Direction.PositiveX), "planner omits occluded cube face");

        ProbeAssertions.Require(plan.CubeFaces.Count(face => face.Block.Value == 30 && face.X == 4 && face.Y == 4 && face.Z == 4) == 5, "planner hides glass face against glass");
        ProbeAssertions.Require(plan.CubeFaces.Any(face => face.Block.Value == 30 && face.Kind == ClientBlockRenderKind.TransparentCube), "planner tags transparent cube face");
        ProbeAssertions.Require(!plan.CubeFaces.Any(face => face.Block.Value == 30 && face.X == 4 && face.Y == 4 && face.Z == 4 && face.Direction == Direction.PositiveX), "planner omits shared glass face");

        ProbeAssertions.Require(plan.SpriteFaces.Count(face => face.Block.Value == 9) == 4, "planner emits four cross sprite faces");
        ProbeAssertions.Require(plan.SpriteFaces.Count(face => face.Block.Value == 22) == 6, "planner emits six solid-base sprite faces");

        ProbeAssertions.Require(plan.FluidBlocks.Any(block => block.Block.Value == 14 && block.Kind == ClientBlockRenderKind.Water && block.Level == 0), "planner defers water block");
        ProbeAssertions.Require(plan.FluidBlocks.Any(block => block.Block.Value == 31 && block.Kind == ClientBlockRenderKind.Lava && block.Level == 0), "planner defers lava block");

        ProbeAssertions.Require(!plan.CubeFaces.Any(face => face.Block.Value == 8), "planner emits no cloud cube faces");
        ProbeAssertions.Require(!plan.SpriteFaces.Any(face => face.Block.Value == 8), "planner emits no cloud sprite faces");
        ProbeAssertions.Require(!plan.FluidBlocks.Any(block => block.Block.Value == 8), "planner emits no cloud fluid blocks");

        ProbeAssertions.Require(plan.CubeFaces.SequenceEqual(repeatedPlan.CubeFaces), "planner cube output is deterministic");
        ProbeAssertions.Require(plan.SpriteFaces.SequenceEqual(repeatedPlan.SpriteFaces), "planner sprite output is deterministic");
        ProbeAssertions.Require(plan.FluidBlocks.SequenceEqual(repeatedPlan.FluidBlocks), "planner fluid output is deterministic");

        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 4, 1), BlockId.Air), "planner source mutates after capture");
        var immutablePlan = planner.Build(snapshot);
        ProbeAssertions.Require(plan.CubeFaces.SequenceEqual(immutablePlan.CubeFaces), "planner cube output uses captured snapshot");
        ProbeAssertions.Require(plan.SpriteFaces.SequenceEqual(immutablePlan.SpriteFaces), "planner sprite output uses captured snapshot");
        ProbeAssertions.Require(plan.FluidBlocks.SequenceEqual(immutablePlan.FluidBlocks), "planner fluid output uses captured snapshot");
    }

    public static void ValidateNonFluidPlannerToPackerPipeline()
    {
        var rules = new ClientBlockRenderRules();
        var planner = new ClientChunkMeshPlanner(rules);
        var packer = new ClientChunkMeshPacker(rules);
        var store = new ClientBlockPresentationStore();

        ProbeAssertions.Require(store.Apply(new BlockPosition(1, 6, 1), new BlockId(1)), "pipeline opaque cube block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(2, 6, 1), new BlockId(5)), "pipeline opaque occluder");
        ProbeAssertions.Require(store.Apply(new BlockPosition(4, 6, 4), new BlockId(30)), "pipeline glass block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(5, 6, 4), new BlockId(30)), "pipeline glass occluder");
        ProbeAssertions.Require(store.Apply(new BlockPosition(8, 6, 8), new BlockId(9)), "pipeline cross sprite block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(10, 6, 10), new BlockId(22)), "pipeline solid-base sprite block");
        ProbeAssertions.Require(store.Apply(new BlockPosition(12, 6, 12), new BlockId(8)), "pipeline hidden block");

        var snapshot = store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            ClientNeighborhoodBoundaryBlocks.Air);
        var plan = planner.Build(snapshot);
        var repeatedPlan = planner.Build(snapshot);
        ProbeAssertions.Require(plan.FluidBlocks.Count == 0, "pipeline keeps non-fluid snapshot fluid-free");
        ProbeAssertions.Require(plan.CubeFaces.Count(face => face.Block.Value == 1 && face.X == 1 && face.Y == 6 && face.Z == 1) == 5, "pipeline plans primary opaque cube faces");
        ProbeAssertions.Require(plan.CubeFaces.Count(face => face.Kind == ClientBlockRenderKind.OpaqueCube) == 10, "pipeline plans opaque cube faces");
        ProbeAssertions.Require(plan.CubeFaces.Count(face => face.Block.Value == 30 && face.X == 4 && face.Y == 6 && face.Z == 4) == 5, "pipeline plans primary transparent cube faces");
        ProbeAssertions.Require(plan.CubeFaces.Count(face => face.Kind == ClientBlockRenderKind.TransparentCube) == 10, "pipeline plans transparent cube faces");
        ProbeAssertions.Require(plan.SpriteFaces.Count == 10, "pipeline plans sprite faces");
        ProbeAssertions.Require(!plan.CubeFaces.Any(face => face.Block.Value == 8), "pipeline skips hidden cube faces");
        ProbeAssertions.Require(!plan.SpriteFaces.Any(face => face.Block.Value == 8), "pipeline skips hidden sprite faces");
        ProbeAssertions.Require(plan.CubeFaces.SequenceEqual(repeatedPlan.CubeFaces), "pipeline cube planning is deterministic");
        ProbeAssertions.Require(plan.SpriteFaces.SequenceEqual(repeatedPlan.SpriteFaces), "pipeline sprite planning is deterministic");

        var packed = packer.Pack(plan);
        var repeatedPacked = packer.Pack(repeatedPlan);
        ProbeAssertions.Require(packed.OpaqueCubeFaces.Count == 10, "pipeline packs opaque faces into opaque bucket");
        ProbeAssertions.Require(packed.TransparentCubeFaces.Count == 10, "pipeline packs glass faces into transparent bucket");
        ProbeAssertions.Require(packed.SpriteVertices.Count == 40, "pipeline packs four vertices per sprite face");
        ProbeAssertions.Require(packed.FluidBlocks.Count == 0, "pipeline preserves empty fluid block list");
        ProbeAssertions.Require(packed.OpaqueCubeFaces.SequenceEqual(repeatedPacked.OpaqueCubeFaces), "pipeline opaque packing is deterministic");
        ProbeAssertions.Require(packed.TransparentCubeFaces.SequenceEqual(repeatedPacked.TransparentCubeFaces), "pipeline transparent packing is deterministic");
        ProbeAssertions.Require(packed.SpriteVertices.SequenceEqual(repeatedPacked.SpriteVertices), "pipeline sprite packing is deterministic");

        var packedOpaqueFace = packed.OpaqueCubeFaces.First();
        ProbeAssertions.Require(ClientPackedCubeFace.X(packedOpaqueFace) == 1, "pipeline packed opaque x");
        ProbeAssertions.Require(ClientPackedCubeFace.Y(packedOpaqueFace) == 6, "pipeline packed opaque y");
        ProbeAssertions.Require(ClientPackedCubeFace.Z(packedOpaqueFace) == 1, "pipeline packed opaque z");
        ProbeAssertions.Require(ClientPackedCubeFace.Direction(packedOpaqueFace) == 0, "pipeline packed opaque first direction");
        ProbeAssertions.Require(ClientPackedCubeFace.AtlasLayer(packedOpaqueFace) == 2, "pipeline packed opaque atlas layer");
        ProbeAssertions.Require(ClientPackedCubeFace.HasOcclusion(packedOpaqueFace), "pipeline packed opaque occlusion");

        var packedSpriteVertex = packed.SpriteVertices.First();
        ProbeAssertions.Require(ClientPackedSpriteVertex.X(packedSpriteVertex) == 8, "pipeline packed sprite x");
        ProbeAssertions.Require(ClientPackedSpriteVertex.Y(packedSpriteVertex) == 6, "pipeline packed sprite y");
        ProbeAssertions.Require(ClientPackedSpriteVertex.Z(packedSpriteVertex) == 8, "pipeline packed sprite z");
        ProbeAssertions.Require(ClientPackedSpriteVertex.PackedDirection(packedSpriteVertex) == 6, "pipeline packed sprite first direction");
        ProbeAssertions.Require(ClientPackedSpriteVertex.AtlasLayer(packedSpriteVertex) == 15, "pipeline packed sprite atlas layer");
        ProbeAssertions.Require(!ClientPackedSpriteVertex.HasOcclusion(packedSpriteVertex), "pipeline packed sprite occlusion");
    }
}
