using System.Runtime.InteropServices;
using Octaryn.Client.WorldPresentation;
using Octaryn.Client.ClientHost;
using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

return ClientWorldPresentationProbe.Run();

internal static class ClientWorldPresentationProbe
{
    public static int Run()
    {
        var store = new ClientBlockPresentationStore();
        var origin = new BlockPosition(1, 5, 1);

        Require(store.GetBlock(origin) == BlockId.Air, "missing block defaults to air");
        Require(store.Apply(origin, new BlockId(7)), "new block updates presentation store");
        Require(store.GetBlock(origin).Value == 7, "new block is visible in presentation store");
        Require(store.PendingUpdateCount == 1, "new block enqueues presentation update");
        Require(store.DirtyChunkCount == 1, "new block marks chunk dirty");

        Require(!store.Apply(origin, new BlockId(7)), "unchanged block does not enqueue update");
        Require(store.PendingUpdateCount == 1, "unchanged block keeps update count stable");

        Require(store.TryDequeueUpdate(out var first), "first update dequeues");
        Require(first.Position == origin, "first update position");
        Require(first.Block.Value == 7, "first update block");
        Require(first.Chunk == new ClientPresentationChunkKey(0, 0, 0), "first update chunk");

        var negative = new BlockPosition(-1, 5, -33);
        Require(store.Apply(negative, new BlockId(8)), "negative block updates presentation store");
        Require(store.TryDequeueUpdate(out var second), "negative update dequeues");
        Require(second.Chunk == new ClientPresentationChunkKey(-1, 0, -2), "negative block uses floor chunk coordinates");

        var initialDirty = store.DrainDirtyChunks();
        Require(initialDirty.Count == 4, "drain returns dirty chunks");
        Require(initialDirty.Contains(first.Chunk), "drain includes first dirty chunk");
        Require(initialDirty.Contains(second.Chunk), "drain includes second dirty chunk");
        Require(initialDirty.Contains(new ClientPresentationChunkKey(0, 0, -2)), "drain includes negative x border chunk");
        Require(initialDirty.Contains(new ClientPresentationChunkKey(-1, 0, -1)), "drain includes negative z border chunk");
        Require(store.DirtyChunkCount == 0, "drain clears dirty chunks");

        var border = new BlockPosition(31, 5, 0);
        Require(store.Apply(border, new BlockId(9)), "border block updates presentation store");
        var borderDirty = store.DrainDirtyChunks();
        Require(borderDirty.Contains(new ClientPresentationChunkKey(0, 0, 0)), "border edit marks owner chunk");
        Require(borderDirty.Contains(new ClientPresentationChunkKey(1, 0, 0)), "border edit marks east chunk");
        Require(borderDirty.Contains(new ClientPresentationChunkKey(0, 0, -1)), "border edit marks north chunk");
        Require(borderDirty.Count == 3, "border edit marks only needed chunks");

        var verticalBorder = new BlockPosition(4, 0, 4);
        Require(store.Apply(verticalBorder, new BlockId(10)), "vertical border block updates presentation store");
        var verticalDirty = store.DrainDirtyChunks();
        Require(verticalDirty.Contains(new ClientPresentationChunkKey(0, 0, 0)), "vertical border marks owner section");
        Require(verticalDirty.Contains(new ClientPresentationChunkKey(0, -1, 0)), "vertical border marks lower section");
        Require(verticalDirty.Count == 2, "vertical border marks only needed sections");

        var bottomStore = new ClientBlockPresentationStore();
        Require(bottomStore.Apply(new BlockPosition(4, ChunkConstants.WorldMinY, 4), new BlockId(11)), "world bottom block updates presentation store");
        var bottomDirty = bottomStore.DrainDirtyChunks();
        Require(bottomDirty.Contains(new ClientPresentationChunkKey(0, ClientPresentationChunkKey.MinSectionY, 0)), "world bottom marks owner section");
        Require(bottomDirty.Count == 1, "world bottom does not mark below-world section");

        var topStore = new ClientBlockPresentationStore();
        Require(topStore.Apply(new BlockPosition(4, ChunkConstants.WorldMaxYExclusive - 1, 4), new BlockId(12)), "world top block updates presentation store");
        var topDirty = topStore.DrainDirtyChunks();
        Require(topDirty.Contains(new ClientPresentationChunkKey(0, ClientPresentationChunkKey.MaxSectionYExclusive - 1, 0)), "world top marks owner section");
        Require(topDirty.Count == 1, "world top does not mark above-world section");

        Require(store.Apply(origin, BlockId.Air), "air removes presented block");
        Require(store.GetBlock(origin) == BlockId.Air, "air removal visible");

        ValidateNeighborhoodSnapshot();
        ValidateBlockRenderRules();
        ValidateNeighborhoodFaceVisibility();
        ValidateChunkMeshPlanner();
        ValidatePackedChunkMesh();
        ValidateNonFluidPlannerToPackerPipeline();
        ValidateNonFluidUploadPlan();
        ValidateSnapshotConsumerTickOrder();
        return 0;
    }

    private static void ValidateNeighborhoodSnapshot()
    {
        var store = new ClientBlockPresentationStore();
        Require(store.Apply(new BlockPosition(1, 4, 1), new BlockId(11)), "snapshot center source block");
        Require(store.Apply(new BlockPosition(32, 4, 1), new BlockId(12)), "snapshot east source block");
        Require(store.Apply(new BlockPosition(-1, 4, 1), new BlockId(13)), "snapshot west source block");
        Require(store.Apply(new BlockPosition(1, 4, 32), new BlockId(14)), "snapshot south source block");
        Require(store.Apply(new BlockPosition(1, 4, -1), new BlockId(15)), "snapshot north source block");
        Require(store.Apply(new BlockPosition(64, 4, 1), new BlockId(16)), "snapshot outside source block");

        var snapshot = store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            new ClientNeighborhoodBoundaryBlocks(new BlockId(1)));
        Require(snapshot.LocalBlock(1, 1, 1, 4, 1).Value == 11, "snapshot center local block");
        Require(snapshot.NeighborhoodBlock(31, 4, 1, 1, 0, 0).Value == 12, "snapshot samples east neighbor");
        Require(snapshot.NeighborhoodBlock(0, 4, 1, -1, 0, 0).Value == 13, "snapshot samples west neighbor");
        Require(snapshot.NeighborhoodBlock(1, 4, 31, 0, 0, 1).Value == 14, "snapshot samples south neighbor");
        Require(snapshot.NeighborhoodBlock(1, 4, 0, 0, 0, -1).Value == 15, "snapshot samples north neighbor");
        Require(snapshot.NeighborhoodBlock(31, 4, 1, 33, 0, 0) == BlockId.Air, "snapshot outside neighborhood is air");
        Require(snapshot.NeighborhoodBlock(1, 0, 1, 0, -1, 0) == BlockId.Air, "snapshot missing lower section is air");
        Require(snapshot.NeighborhoodBlock(1, ClientChunkNeighborhoodSnapshot.Height - 1, 1, 0, 1, 0) == BlockId.Air, "snapshot missing upper section is air");

        Require(store.Apply(new BlockPosition(1, 4, 1), new BlockId(20)), "snapshot source mutates after capture");
        Require(snapshot.LocalBlock(1, 1, 1, 4, 1).Value == 11, "snapshot is immutable after capture");

        var worldBottomStore = new ClientBlockPresentationStore();
        Require(worldBottomStore.Apply(new BlockPosition(1, ChunkConstants.WorldMinY, 1), new BlockId(21)), "snapshot world bottom source block");
        var worldBottom = worldBottomStore.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, ChunkConstants.WorldMinY / ClientChunkNeighborhoodSnapshot.Height, 0),
            new ClientNeighborhoodBoundaryBlocks(new BlockId(1)));
        Require(worldBottom.LocalBlock(1, 1, 1, 0, 1).Value == 21, "snapshot captures centered world bottom block");
        Require(worldBottom.NeighborhoodBlock(1, 0, 1, 0, -1, 0).Value == 1, "snapshot below world uses boundary block");

        var worldTopStore = new ClientBlockPresentationStore();
        Require(worldTopStore.Apply(new BlockPosition(1, ChunkConstants.WorldMaxYExclusive - 1, 1), new BlockId(22)), "snapshot world top source block");
        var worldTop = worldTopStore.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, (ChunkConstants.WorldMaxYExclusive - 1) / ClientChunkNeighborhoodSnapshot.Height, 0),
            new ClientNeighborhoodBoundaryBlocks(new BlockId(1)));
        Require(worldTop.LocalBlock(1, 1, 1, ClientChunkNeighborhoodSnapshot.Height - 1, 1).Value == 22, "snapshot captures centered world top block");
        Require(worldTop.NeighborhoodBlock(1, ClientChunkNeighborhoodSnapshot.Height - 1, 1, 0, 1, 0) == BlockId.Air, "snapshot above world is air");
    }

    private static void ValidateBlockRenderRules()
    {
        var rules = new ClientBlockRenderRules();

        Require(rules.RenderKind(BlockId.Air) == ClientBlockRenderKind.Empty, "air has no render mesh");
        Require(!rules.ShouldBuildCubeFaces(BlockId.Air), "air has no cube faces");

        var grass = rules.Properties(new BlockId(1));
        Require(grass.Kind == ClientBlockRenderKind.OpaqueCube, "grass renders as opaque cube");
        Require(grass.IsOpaque, "grass is opaque");
        Require(grass.HasOcclusion, "grass occludes faces");

        var leaves = rules.Properties(new BlockId(7));
        Require(leaves.Kind == ClientBlockRenderKind.OpaqueCube, "leaves render as opaque cube");
        Require(leaves.IsOpaque, "leaves are opaque");
        Require(!leaves.HasOcclusion, "leaves do not occlude neighbor faces");

        var cloud = rules.Properties(new BlockId(8));
        Require(cloud.Kind == ClientBlockRenderKind.Hidden, "cloud has no emitted render mesh");
        Require(!rules.ShouldBuildCubeFaces(new BlockId(8)), "cloud skips cube faces");

        var bush = rules.Properties(new BlockId(9));
        Require(bush.Kind == ClientBlockRenderKind.Sprite, "bush renders as sprite");
        Require(bush.IsSprite, "bush is sprite");
        Require(!bush.HasOcclusion, "bush does not occlude neighbor faces");

        var waterSource = rules.Properties(new BlockId(14));
        Require(waterSource.Kind == ClientBlockRenderKind.Water, "water source renders as water");
        Require(waterSource.IsFluid, "water source is fluid");
        Require(waterSource.FluidLevel == 0, "water source fluid level");

        var waterFlow = rules.Properties(new BlockId(21));
        Require(waterFlow.Kind == ClientBlockRenderKind.Water, "flowing water renders as water");
        Require(waterFlow.FluidLevel == 7, "flowing water max fluid level");

        var glass = rules.Properties(new BlockId(30));
        Require(glass.Kind == ClientBlockRenderKind.TransparentCube, "glass renders as transparent cube");
        Require(!glass.HasOcclusion, "glass does not occlude neighbor faces");

        var lavaSource = rules.Properties(new BlockId(31));
        Require(lavaSource.Kind == ClientBlockRenderKind.Lava, "lava source renders as lava");
        Require(lavaSource.IsFluid, "lava source is fluid");
        Require(lavaSource.FluidLevel == 0, "lava source fluid level");

        var lavaFlow = rules.Properties(new BlockId(38));
        Require(lavaFlow.Kind == ClientBlockRenderKind.Lava, "flowing lava renders as lava");
        Require(lavaFlow.FluidLevel == 7, "flowing lava max fluid level");

        Require(rules.IsCubeFaceVisible(new BlockId(1), BlockId.Air), "air exposes opaque face");
        Require(!rules.IsCubeFaceVisible(new BlockId(1), new BlockId(5)), "opaque neighbor hides opaque face");
        Require(rules.IsCubeFaceVisible(new BlockId(1), new BlockId(9)), "sprite neighbor exposes opaque face");
        Require(rules.IsCubeFaceVisible(new BlockId(1), new BlockId(30)), "glass neighbor exposes opaque face");
        Require(!rules.IsCubeFaceVisible(new BlockId(30), new BlockId(30)), "glass neighbor hides glass face");
        Require(rules.IsCubeFaceVisible(new BlockId(30), new BlockId(7)), "leaves expose glass face");
        Require(rules.IsCubeFaceVisible(new BlockId(7), new BlockId(7)), "leaves neighbor keeps leaf face visible");
        Require(!rules.IsCubeFaceVisible(new BlockId(8), BlockId.Air), "cloud emits no face against air");
        Require(!rules.IsCubeFaceVisible(new BlockId(14), BlockId.Air), "water does not use cube face rule");
    }

    private static void ValidateNeighborhoodFaceVisibility()
    {
        var rules = new ClientBlockRenderRules();
        var store = new ClientBlockPresentationStore();
        Require(store.Apply(new BlockPosition(31, 5, 1), new BlockId(1)), "east boundary center block");
        Require(store.Apply(new BlockPosition(32, 5, 1), new BlockId(5)), "east boundary opaque neighbor");
        Require(store.Apply(new BlockPosition(0, 5, 1), new BlockId(1)), "west boundary center block");
        Require(store.Apply(new BlockPosition(-1, 5, 1), new BlockId(7)), "west boundary leaves neighbor");
        Require(store.Apply(new BlockPosition(1, 5, 31), new BlockId(1)), "south boundary center block");
        Require(store.Apply(new BlockPosition(1, 5, 32), new BlockId(30)), "south boundary glass neighbor");
        Require(store.Apply(new BlockPosition(1, 5, 0), new BlockId(1)), "north boundary center block");
        Require(store.Apply(new BlockPosition(1, 5, -1), new BlockId(9)), "north boundary sprite neighbor");
        Require(store.Apply(new BlockPosition(4, ClientChunkNeighborhoodSnapshot.Height - 1, 4), new BlockId(1)), "top face center block");
        Require(store.Apply(new BlockPosition(5, 0, 5), new BlockId(1)), "bottom face center block");
        Require(store.Apply(new BlockPosition(5, -1, 5), new BlockId(1)), "lower section bottom occluder");

        var snapshot = store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            new ClientNeighborhoodBoundaryBlocks(new BlockId(1)));

        Require(!ContainsFace(rules, snapshot, 31, 5, 1, Direction.PositiveX), "opaque east neighbor hides boundary face");
        Require(ContainsFace(rules, snapshot, 0, 5, 1, Direction.NegativeX), "leaves west neighbor exposes boundary face");
        Require(ContainsFace(rules, snapshot, 1, 5, 31, Direction.PositiveZ), "glass south neighbor exposes boundary face");
        Require(ContainsFace(rules, snapshot, 1, 5, 0, Direction.NegativeZ), "sprite north neighbor exposes boundary face");
        Require(ContainsFace(rules, snapshot, 4, ClientChunkNeighborhoodSnapshot.Height - 1, 4, Direction.PositiveY), "missing upper section exposes top face");
        Require(!ContainsFace(rules, snapshot, 5, 0, 5, Direction.NegativeY), "lower section block hides bottom face");

        var airBelowStore = new ClientBlockPresentationStore();
        Require(airBelowStore.Apply(new BlockPosition(5, 0, 5), new BlockId(1)), "air lower section center block");
        var airBelowSnapshot = airBelowStore.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            new ClientNeighborhoodBoundaryBlocks(BlockId.Air));
        Require(ContainsFace(rules, airBelowSnapshot, 5, 0, 5, Direction.NegativeY), "missing lower section exposes bottom face");
    }

    private static void ValidateChunkMeshPlanner()
    {
        var rules = new ClientBlockRenderRules();
        var planner = new ClientChunkMeshPlanner(rules);
        var store = new ClientBlockPresentationStore();

        Require(store.Apply(new BlockPosition(1, 4, 1), new BlockId(1)), "planner opaque cube block");
        Require(store.Apply(new BlockPosition(2, 4, 1), new BlockId(5)), "planner opaque cube occluder");
        Require(store.Apply(new BlockPosition(4, 4, 4), new BlockId(30)), "planner glass block");
        Require(store.Apply(new BlockPosition(5, 4, 4), new BlockId(30)), "planner glass neighbor");
        Require(store.Apply(new BlockPosition(8, 4, 8), new BlockId(9)), "planner cross sprite block");
        Require(store.Apply(new BlockPosition(10, 4, 10), new BlockId(22)), "planner solid-base sprite block");
        Require(store.Apply(new BlockPosition(12, 4, 12), new BlockId(14)), "planner water block");
        Require(store.Apply(new BlockPosition(14, 4, 14), new BlockId(31)), "planner lava block");
        Require(store.Apply(new BlockPosition(16, 4, 16), new BlockId(8)), "planner cloud block");

        var snapshot = store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            new ClientNeighborhoodBoundaryBlocks(BlockId.Air));
        var plan = planner.Build(snapshot);
        var repeatedPlan = planner.Build(snapshot);

        Require(plan.CubeFaces.Count(face => face.Block.Value == 1) == 5, "planner hides cube face against opaque neighbor");
        Require(plan.CubeFaces.First(face => face.Block.Value == 1).Direction == Direction.PositiveZ, "planner preserves cube face order");
        Require(plan.CubeFaces.Any(face => face.Block.Value == 1 && face.Direction == Direction.NegativeX), "planner keeps visible cube face");
        Require(plan.CubeFaces.Any(face => face.Block.Value == 1 && face.Kind == ClientBlockRenderKind.OpaqueCube), "planner tags opaque cube face");
        Require(!plan.CubeFaces.Any(face => face.Block.Value == 1 && face.Direction == Direction.PositiveX), "planner omits occluded cube face");

        Require(plan.CubeFaces.Count(face => face.Block.Value == 30 && face.X == 4 && face.Y == 4 && face.Z == 4) == 5, "planner hides glass face against glass");
        Require(plan.CubeFaces.Any(face => face.Block.Value == 30 && face.Kind == ClientBlockRenderKind.TransparentCube), "planner tags transparent cube face");
        Require(!plan.CubeFaces.Any(face => face.Block.Value == 30 && face.X == 4 && face.Y == 4 && face.Z == 4 && face.Direction == Direction.PositiveX), "planner omits shared glass face");

        Require(plan.SpriteFaces.Count(face => face.Block.Value == 9) == 4, "planner emits four cross sprite faces");
        Require(plan.SpriteFaces.Count(face => face.Block.Value == 22) == 6, "planner emits six solid-base sprite faces");

        Require(plan.FluidBlocks.Any(block => block.Block.Value == 14 && block.Kind == ClientBlockRenderKind.Water && block.Level == 0), "planner defers water block");
        Require(plan.FluidBlocks.Any(block => block.Block.Value == 31 && block.Kind == ClientBlockRenderKind.Lava && block.Level == 0), "planner defers lava block");

        Require(!plan.CubeFaces.Any(face => face.Block.Value == 8), "planner emits no cloud cube faces");
        Require(!plan.SpriteFaces.Any(face => face.Block.Value == 8), "planner emits no cloud sprite faces");
        Require(!plan.FluidBlocks.Any(block => block.Block.Value == 8), "planner emits no cloud fluid blocks");

        Require(plan.CubeFaces.SequenceEqual(repeatedPlan.CubeFaces), "planner cube output is deterministic");
        Require(plan.SpriteFaces.SequenceEqual(repeatedPlan.SpriteFaces), "planner sprite output is deterministic");
        Require(plan.FluidBlocks.SequenceEqual(repeatedPlan.FluidBlocks), "planner fluid output is deterministic");

        Require(store.Apply(new BlockPosition(1, 4, 1), BlockId.Air), "planner source mutates after capture");
        var immutablePlan = planner.Build(snapshot);
        Require(plan.CubeFaces.SequenceEqual(immutablePlan.CubeFaces), "planner cube output uses captured snapshot");
        Require(plan.SpriteFaces.SequenceEqual(immutablePlan.SpriteFaces), "planner sprite output uses captured snapshot");
        Require(plan.FluidBlocks.SequenceEqual(immutablePlan.FluidBlocks), "planner fluid output uses captured snapshot");
    }

    private static void ValidatePackedChunkMesh()
    {
        var rules = new ClientBlockRenderRules();
        var packer = new ClientChunkMeshPacker(rules);

        Require(rules.AtlasLayer(new BlockId(1), Direction.PositiveY) == 1, "grass top atlas layer");
        Require(rules.AtlasLayer(new BlockId(1), Direction.NegativeY) == 3, "grass bottom atlas layer");
        Require(rules.AtlasLayer(new BlockId(6), Direction.PositiveX) == 8, "log side atlas layer");
        Require(rules.AtlasLayer(new BlockId(6), Direction.PositiveY) == 7, "log top atlas layer");

        Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.PositiveZ) == 0, "positive z maps to old north");
        Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.NegativeZ) == 1, "negative z maps to old south");
        Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.PositiveX) == 2, "positive x maps to old east");
        Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.NegativeX) == 3, "negative x maps to old west");
        Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.PositiveY) == 4, "positive y maps to old up");
        Require((int)ClientPackedMeshDirectionMap.FromDirection(Direction.NegativeY) == 5, "negative y maps to old down");

        var boundaryFace = new ClientCubeMeshFace(
            new BlockId(1),
            ClientBlockRenderKind.OpaqueCube,
            31,
            255,
            31,
            Direction.PositiveY);
        var packedBoundaryFace = ClientPackedCubeFace.Pack(boundaryFace, rules);
        Require(ClientPackedCubeFace.X(packedBoundaryFace) == 31, "packed cube x field");
        Require(ClientPackedCubeFace.Y(packedBoundaryFace) == 255, "packed cube y field");
        Require(ClientPackedCubeFace.Z(packedBoundaryFace) == 31, "packed cube z field");
        Require(ClientPackedCubeFace.Direction(packedBoundaryFace) == 4, "packed cube direction field");
        Require(ClientPackedCubeFace.UExtent(packedBoundaryFace) == 1, "packed cube u extent");
        Require(ClientPackedCubeFace.VExtent(packedBoundaryFace) == 1, "packed cube v extent");
        Require(ClientPackedCubeFace.AtlasLayer(packedBoundaryFace) == 1, "packed cube atlas layer");
        Require(ClientPackedCubeFace.HasOcclusion(packedBoundaryFace), "packed cube occlusion bit");
        Require(ClientPackedCubeFace.ChunkSlot(packedBoundaryFace) == ClientPackedCubeFace.UnsetChunkSlot, "packed cube unset chunk slot");
        Require(ClientPackedCubeFace.WaterLevel(packedBoundaryFace) == 0, "packed cube water level default");
        Require(!ClientPackedCubeFace.IsWater(packedBoundaryFace), "packed cube water flag default");
        Require(ClientPackedCubeFace.WaterBaseHeight(packedBoundaryFace) == 0, "packed cube water base default");

        var glassFace = new ClientCubeMeshFace(
            new BlockId(30),
            ClientBlockRenderKind.TransparentCube,
            4,
            4,
            4,
            Direction.NegativeZ);
        var packedGlassFace = ClientPackedCubeFace.Pack(glassFace, rules);
        Require(ClientPackedCubeFace.Direction(packedGlassFace) == 1, "packed glass direction field");
        Require(ClientPackedCubeFace.AtlasLayer(packedGlassFace) == 25, "packed glass atlas layer");
        Require(!ClientPackedCubeFace.HasOcclusion(packedGlassFace), "packed glass occlusion bit");

        var isolatedFaces = new[]
        {
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.PositiveZ),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.NegativeZ),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.PositiveX),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.NegativeX),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.PositiveY),
            new ClientCubeMeshFace(new BlockId(1), ClientBlockRenderKind.OpaqueCube, 6, 7, 8, Direction.NegativeY)
        };
        var packedIsolatedFaces = packer.Pack(new ClientChunkMeshPlan(isolatedFaces, [], []));
        Require(packedIsolatedFaces.OpaqueCubeFaces.Select(ClientPackedCubeFace.Direction).SequenceEqual([0, 1, 2, 3, 4, 5]), "packed cube face sequence matches old order");

        var spriteFace = new ClientSpriteMeshFace(new BlockId(9), 8, 4, 8, Direction.PositiveZ);
        var packedSprite0 = ClientPackedSpriteVertex.Pack(spriteFace, rules, 0);
        var packedSprite1 = ClientPackedSpriteVertex.Pack(spriteFace, rules, 1);
        Require(ClientPackedSpriteVertex.PackedDirection(packedSprite0) == 6, "packed sprite direction starts after cube directions");
        Require(ClientPackedSpriteVertex.AtlasLayer(packedSprite0) == 15, "packed sprite atlas layer");
        Require(ClientPackedSpriteVertex.U(packedSprite0) == 1, "packed sprite first u");
        Require(ClientPackedSpriteVertex.V(packedSprite0) == 1, "packed sprite first v");
        Require(ClientPackedSpriteVertex.U(packedSprite1) == 1, "packed sprite second u");
        Require(ClientPackedSpriteVertex.V(packedSprite1) == 0, "packed sprite second v");

        var plan = new ClientChunkMeshPlan(
            [boundaryFace],
            [
                new ClientSpriteMeshFace(new BlockId(9), 8, 4, 8, Direction.PositiveZ),
                new ClientSpriteMeshFace(new BlockId(22), 10, 4, 10, Direction.NegativeY)
            ],
            [new ClientFluidMeshBlock(new BlockId(14), ClientBlockRenderKind.Water, 12, 4, 12, 0)]);
        var packed = packer.Pack(plan);
        var repeatedPacked = packer.Pack(plan);
        Require(packed.OpaqueCubeFaces.Count == 1, "packer emits opaque cube bucket");
        Require(packed.TransparentCubeFaces.Count == 0, "packer keeps transparent bucket empty");
        Require(packed.SpriteVertices.Count == 8, "packer emits four vertices per sprite face");
        Require(packed.FluidBlocks.Count == 1, "packer preserves deferred fluid blocks");
        Require(packed.OpaqueCubeFaces.SequenceEqual(repeatedPacked.OpaqueCubeFaces), "packed opaque output is deterministic");
        Require(packed.TransparentCubeFaces.SequenceEqual(repeatedPacked.TransparentCubeFaces), "packed transparent output is deterministic");
        Require(packed.SpriteVertices.SequenceEqual(repeatedPacked.SpriteVertices), "packed sprite output is deterministic");
        Require(packed.FluidBlocks.SequenceEqual(repeatedPacked.FluidBlocks), "packed fluid output is deterministic");

        var transparentPlan = new ClientChunkMeshPlan([glassFace], [], []);
        var packedTransparent = packer.Pack(transparentPlan);
        Require(packedTransparent.OpaqueCubeFaces.Count == 0, "packer keeps glass out of opaque bucket");
        Require(packedTransparent.TransparentCubeFaces.Count == 1, "packer emits transparent cube bucket");
    }

    private static void ValidateNonFluidPlannerToPackerPipeline()
    {
        var rules = new ClientBlockRenderRules();
        var planner = new ClientChunkMeshPlanner(rules);
        var packer = new ClientChunkMeshPacker(rules);
        var store = new ClientBlockPresentationStore();

        Require(store.Apply(new BlockPosition(1, 6, 1), new BlockId(1)), "pipeline opaque cube block");
        Require(store.Apply(new BlockPosition(2, 6, 1), new BlockId(5)), "pipeline opaque occluder");
        Require(store.Apply(new BlockPosition(4, 6, 4), new BlockId(30)), "pipeline glass block");
        Require(store.Apply(new BlockPosition(5, 6, 4), new BlockId(30)), "pipeline glass occluder");
        Require(store.Apply(new BlockPosition(8, 6, 8), new BlockId(9)), "pipeline cross sprite block");
        Require(store.Apply(new BlockPosition(10, 6, 10), new BlockId(22)), "pipeline solid-base sprite block");
        Require(store.Apply(new BlockPosition(12, 6, 12), new BlockId(8)), "pipeline hidden block");

        var snapshot = store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            new ClientNeighborhoodBoundaryBlocks(BlockId.Air));
        var plan = planner.Build(snapshot);
        var repeatedPlan = planner.Build(snapshot);
        Require(plan.FluidBlocks.Count == 0, "pipeline keeps non-fluid snapshot fluid-free");
        Require(plan.CubeFaces.Count(face => face.Block.Value == 1 && face.X == 1 && face.Y == 6 && face.Z == 1) == 5, "pipeline plans primary opaque cube faces");
        Require(plan.CubeFaces.Count(face => face.Kind == ClientBlockRenderKind.OpaqueCube) == 10, "pipeline plans opaque cube faces");
        Require(plan.CubeFaces.Count(face => face.Block.Value == 30 && face.X == 4 && face.Y == 6 && face.Z == 4) == 5, "pipeline plans primary transparent cube faces");
        Require(plan.CubeFaces.Count(face => face.Kind == ClientBlockRenderKind.TransparentCube) == 10, "pipeline plans transparent cube faces");
        Require(plan.SpriteFaces.Count == 10, "pipeline plans sprite faces");
        Require(!plan.CubeFaces.Any(face => face.Block.Value == 8), "pipeline skips hidden cube faces");
        Require(!plan.SpriteFaces.Any(face => face.Block.Value == 8), "pipeline skips hidden sprite faces");
        Require(plan.CubeFaces.SequenceEqual(repeatedPlan.CubeFaces), "pipeline cube planning is deterministic");
        Require(plan.SpriteFaces.SequenceEqual(repeatedPlan.SpriteFaces), "pipeline sprite planning is deterministic");

        var packed = packer.Pack(plan);
        var repeatedPacked = packer.Pack(repeatedPlan);
        Require(packed.OpaqueCubeFaces.Count == 10, "pipeline packs opaque faces into opaque bucket");
        Require(packed.TransparentCubeFaces.Count == 10, "pipeline packs glass faces into transparent bucket");
        Require(packed.SpriteVertices.Count == 40, "pipeline packs four vertices per sprite face");
        Require(packed.FluidBlocks.Count == 0, "pipeline preserves empty fluid block list");
        Require(packed.OpaqueCubeFaces.SequenceEqual(repeatedPacked.OpaqueCubeFaces), "pipeline opaque packing is deterministic");
        Require(packed.TransparentCubeFaces.SequenceEqual(repeatedPacked.TransparentCubeFaces), "pipeline transparent packing is deterministic");
        Require(packed.SpriteVertices.SequenceEqual(repeatedPacked.SpriteVertices), "pipeline sprite packing is deterministic");

        var packedOpaqueFace = packed.OpaqueCubeFaces.First();
        Require(ClientPackedCubeFace.X(packedOpaqueFace) == 1, "pipeline packed opaque x");
        Require(ClientPackedCubeFace.Y(packedOpaqueFace) == 6, "pipeline packed opaque y");
        Require(ClientPackedCubeFace.Z(packedOpaqueFace) == 1, "pipeline packed opaque z");
        Require(ClientPackedCubeFace.Direction(packedOpaqueFace) == 0, "pipeline packed opaque first direction");
        Require(ClientPackedCubeFace.AtlasLayer(packedOpaqueFace) == 2, "pipeline packed opaque atlas layer");
        Require(ClientPackedCubeFace.HasOcclusion(packedOpaqueFace), "pipeline packed opaque occlusion");

        var packedSpriteVertex = packed.SpriteVertices.First();
        Require(ClientPackedSpriteVertex.X(packedSpriteVertex) == 8, "pipeline packed sprite x");
        Require(ClientPackedSpriteVertex.Y(packedSpriteVertex) == 6, "pipeline packed sprite y");
        Require(ClientPackedSpriteVertex.Z(packedSpriteVertex) == 8, "pipeline packed sprite z");
        Require(ClientPackedSpriteVertex.PackedDirection(packedSpriteVertex) == 6, "pipeline packed sprite first direction");
        Require(ClientPackedSpriteVertex.AtlasLayer(packedSpriteVertex) == 15, "pipeline packed sprite atlas layer");
        Require(!ClientPackedSpriteVertex.HasOcclusion(packedSpriteVertex), "pipeline packed sprite occlusion");
    }

    private static void ValidateNonFluidUploadPlan()
    {
        Require(
            Marshal.SizeOf<ClientPackedMeshUploadDescriptor>() == ClientPackedMeshUploadDescriptor.SizeValue,
            "upload descriptor managed ABI size");
        Require(
            Marshal.SizeOf<ClientChunkMeshUploadRecord>() == ClientChunkMeshUploadRecord.SizeValue,
            "chunk mesh upload record managed ABI size");

        var empty = ClientPackedMeshUploadValidator.CreateNonFluidPlan(new ClientPackedChunkMesh([], [], [], []));
        Require(empty.OpaqueFaceCount == 0, "empty upload opaque count");
        Require(empty.TransparentFaceCount == 0, "empty upload transparent count");
        Require(empty.SpriteVertexCount == 0, "empty upload sprite vertex count");
        Require(empty.SpriteIndexCount == 0, "empty upload sprite index count");
        Require(empty.ClearsOpaqueFaces, "empty upload clears opaque");
        Require(empty.ClearsTransparentFaces, "empty upload clears transparent");
        Require(empty.ClearsSpriteVertices, "empty upload clears sprites");
        Require(!empty.RequiresUploadSubmit, "empty upload does not require submit");
        var emptyDescriptor = empty.ToDescriptor();
        Require(emptyDescriptor.Version == ClientPackedMeshUploadDescriptor.VersionValue, "empty upload descriptor version");
        Require(emptyDescriptor.Size == ClientPackedMeshUploadDescriptor.SizeValue, "empty upload descriptor size");
        Require(emptyDescriptor.OpaqueFaceCount == 0, "empty upload descriptor opaque count");
        Require(emptyDescriptor.TransparentFaceCount == 0, "empty upload descriptor transparent count");
        Require(emptyDescriptor.SpriteVertexCount == 0, "empty upload descriptor sprite vertex count");
        Require(emptyDescriptor.SpriteIndexCount == 0, "empty upload descriptor sprite index count");
        Require(emptyDescriptor.OpaqueByteCount == 0, "empty upload descriptor opaque bytes");
        Require(emptyDescriptor.TransparentByteCount == 0, "empty upload descriptor transparent bytes");
        Require(emptyDescriptor.SpriteByteCount == 0, "empty upload descriptor sprite bytes");
        Require(emptyDescriptor.Flags == (
            ClientPackedMeshUploadDescriptor.ClearOpaqueFacesFlag |
            ClientPackedMeshUploadDescriptor.ClearTransparentFacesFlag |
            ClientPackedMeshUploadDescriptor.ClearSpriteVerticesFlag), "empty upload descriptor clear flags");

        var rules = new ClientBlockRenderRules();
        var planner = new ClientChunkMeshPlanner(rules);
        var packer = new ClientChunkMeshPacker(rules);
        var store = new ClientBlockPresentationStore();
        Require(store.Apply(new BlockPosition(1, 6, 1), new BlockId(1)), "upload plan opaque block");
        Require(store.Apply(new BlockPosition(4, 6, 4), new BlockId(30)), "upload plan transparent block");
        Require(store.Apply(new BlockPosition(8, 6, 8), new BlockId(9)), "upload plan sprite block");

        var packed = packer.Pack(planner.Build(store.CaptureNeighborhood(
            new ClientPresentationChunkKey(0, 0, 0),
            new ClientNeighborhoodBoundaryBlocks(BlockId.Air))));
        var upload = ClientPackedMeshUploadValidator.CreateNonFluidPlan(packed);
        Require(upload.OpaqueFaceCount == packed.OpaqueCubeFaces.Count, "upload plan opaque count");
        Require(upload.TransparentFaceCount == packed.TransparentCubeFaces.Count, "upload plan transparent count");
        Require(upload.SpriteVertexCount == packed.SpriteVertices.Count, "upload plan sprite vertex count");
        Require(upload.SpriteIndexCount == 24, "upload plan sprite index count");
        Require(!upload.ClearsOpaqueFaces, "upload plan keeps opaque");
        Require(!upload.ClearsTransparentFaces, "upload plan keeps transparent");
        Require(!upload.ClearsSpriteVertices, "upload plan keeps sprites");
        Require(upload.RequiresUploadSubmit, "upload plan requires submit");
        Require(upload.OpaqueByteCount == (ulong)packed.OpaqueCubeFaces.Count * sizeof(ulong), "upload plan opaque byte count");
        Require(upload.TransparentByteCount == (ulong)packed.TransparentCubeFaces.Count * sizeof(ulong), "upload plan transparent byte count");
        Require(upload.SpriteByteCount == (ulong)packed.SpriteVertices.Count * sizeof(uint), "upload plan sprite byte count");
        var descriptor = upload.ToDescriptor();
        Require(descriptor.OpaqueFaceCount == (uint)upload.OpaqueFaceCount, "upload descriptor opaque count");
        Require(descriptor.TransparentFaceCount == (uint)upload.TransparentFaceCount, "upload descriptor transparent count");
        Require(descriptor.SpriteVertexCount == (uint)upload.SpriteVertexCount, "upload descriptor sprite vertex count");
        Require(descriptor.SpriteIndexCount == (uint)upload.SpriteIndexCount, "upload descriptor sprite index count");
        Require(descriptor.OpaqueByteCount == upload.OpaqueByteCount, "upload descriptor opaque bytes");
        Require(descriptor.TransparentByteCount == upload.TransparentByteCount, "upload descriptor transparent bytes");
        Require(descriptor.SpriteByteCount == upload.SpriteByteCount, "upload descriptor sprite bytes");
        Require(descriptor.Flags == 0, "upload descriptor has no clear flags");

        var record = ClientChunkMeshUploadRecord.Create(
            new ClientPresentationChunkKey(1, 2, 3),
            packed,
            opaqueFaceOffset: 4,
            transparentFaceOffset: 8,
            spriteVertexOffset: 12);
        Require(record.Version == ClientChunkMeshUploadRecord.VersionValue, "chunk upload record version");
        Require(record.Size == ClientChunkMeshUploadRecord.SizeValue, "chunk upload record size");
        Require(record.ChunkX == 1 && record.ChunkY == 2 && record.ChunkZ == 3, "chunk upload record key");
        Require(record.OpaqueFaceCount == (uint)packed.OpaqueCubeFaces.Count, "chunk upload record opaque count");
        Require(record.TransparentFaceCount == (uint)packed.TransparentCubeFaces.Count, "chunk upload record transparent count");
        Require(record.SpriteVertexCount == (uint)packed.SpriteVertices.Count, "chunk upload record sprite count");
        Require(record.SpriteIndexCount == 24, "chunk upload record sprite indices");
        Require(record.FluidBlockCount == 0, "chunk upload record fluid count");
        Require(record.OpaqueFaceOffset == 4, "chunk upload record opaque offset");
        Require(record.TransparentFaceOffset == 8, "chunk upload record transparent offset");
        Require(record.SpriteVertexOffset == 12, "chunk upload record sprite offset");
        Require(record.OpaqueByteCount == upload.OpaqueByteCount, "chunk upload record opaque bytes");
        Require(record.TransparentByteCount == upload.TransparentByteCount, "chunk upload record transparent bytes");
        Require(record.SpriteByteCount == upload.SpriteByteCount, "chunk upload record sprite bytes");
        Require(record.Flags == ClientChunkMeshUploadRecord.ClearFluidBlocksFlag, "chunk upload record fluid clear flag");

        RequireThrows<InvalidOperationException>(
            () => ClientPackedMeshUploadValidator.CreateNonFluidPlan(new ClientPackedChunkMesh([], [], [1, 2, 3], [])),
            "upload plan rejects incomplete sprite quad");
        RequireThrows<InvalidOperationException>(
            () => ClientPackedMeshUploadValidator.CreateNonFluidPlan(new ClientPackedChunkMesh(
                [],
                [],
                [],
                [new ClientFluidMeshBlock(new BlockId(14), ClientBlockRenderKind.Water, 1, 2, 3, 0)])),
            "upload plan rejects fluids");
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

    private static unsafe void ValidateSnapshotConsumerTickOrder()
    {
        var store = new ClientBlockPresentationStore();
        var consumer = new ClientServerSnapshotConsumer(store);
        var changes = stackalloc ReplicationChange[1];
        changes[0] = new BlockReplicationChange(new BlockPosition(2, 3, 4), new BlockId(5))
            .ToReplicationChange(10);

        var current = new ServerSnapshotHeader(0, 1, 10, 0, (ulong)changes);
        Require(consumer.Apply(&current) == 0, "consumer accepts current snapshot");
        Require(store.GetBlock(new BlockPosition(2, 3, 4)).Value == 5, "consumer applies current snapshot");

        var stale = new ServerSnapshotHeader(0, 1, 9, 0, (ulong)changes);
        Require(consumer.Apply(&stale) == -3, "consumer rejects stale snapshot");

        var emptyNewer = new ServerSnapshotHeader(0, 0, 12, 0, 0);
        Require(consumer.Apply(&emptyNewer) == 0, "consumer accepts empty newer snapshot");

        var staleAfterEmpty = new ServerSnapshotHeader(0, 1, 11, 0, (ulong)changes);
        Require(consumer.Apply(&staleAfterEmpty) == -3, "consumer rejects change older than empty snapshot");

        var emptyStale = new ServerSnapshotHeader(0, 0, 11, 0, 0);
        Require(consumer.Apply(&emptyStale) == -3, "consumer rejects empty stale snapshot");
    }

    private static void Require(bool condition, string name)
    {
        if (!condition)
        {
            throw new InvalidOperationException(name);
        }
    }

    private static void RequireThrows<TException>(Action action, string name)
        where TException : Exception
    {
        try
        {
            action();
        }
        catch (TException)
        {
            return;
        }

        throw new InvalidOperationException(name);
    }
}
