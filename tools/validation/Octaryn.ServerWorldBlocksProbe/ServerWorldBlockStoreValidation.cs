using Octaryn.Basegame.Gameplay.Interaction;
using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    private static void ValidateWorldConstants()
    {
        Require(ChunkConstants.Width == 32, "chunk width");
        Require(ChunkConstants.Depth == 32, "chunk depth");
        Require(ChunkConstants.SectionHeight == 32, "chunk section height");
        Require(ChunkConstants.WorldHeight == 512, "world height");
        var centeredWorldMinY = -ChunkConstants.WorldHeight / 2;
        var centeredWorldMaxYExclusive = centeredWorldMinY + ChunkConstants.WorldHeight;
        Require(ChunkConstants.WorldMinY == centeredWorldMinY, "centered world min y");
        Require(ChunkConstants.WorldMaxYExclusive == centeredWorldMaxYExclusive, "centered world max y");
        Require(ChunkConstants.WorldMaxYExclusive - ChunkConstants.WorldMinY == ChunkConstants.WorldHeight, "world height span");
        Require(ChunkConstants.SectionBlockCount == ChunkConstants.Width * ChunkConstants.SectionHeight * ChunkConstants.Depth, "section block count");
        Require(ChunkConstants.WorldHeight % ChunkConstants.SectionHeight == 0, "world height section alignment");
        Require(ChunkConstants.WorldHeight != ChunkConstants.SectionHeight, "world height independent from section height");

        Require(BlockLimits.ChunkWidth == ChunkConstants.Width, "server chunk width mirrors shared contract");
        Require(BlockLimits.ChunkDepth == ChunkConstants.Depth, "server chunk depth mirrors shared contract");
        Require(BlockLimits.ChunkSectionHeight == ChunkConstants.SectionHeight, "server section height mirrors shared contract");
        Require(BlockLimits.WorldHeight == ChunkConstants.WorldHeight, "server world height mirrors shared contract");
        Require(BlockLimits.WorldMinY == ChunkConstants.WorldMinY, "server min y mirrors shared contract");
        Require(BlockLimits.WorldMaxYExclusive == ChunkConstants.WorldMaxYExclusive, "server max y mirrors shared contract");
    }

    private static void ValidateEditAndQuery()
    {
        var store = new BlockStore();
        var service = new BlockEditService(store, new BlockAuthorityRules());
        var position = new BlockPosition(1, 2, 3);

        Require(service.GetBlock(position) == BlockId.Air, "missing block returns air");

        var result = service.Apply(new BlockEdit(position, new BlockId(5)));
        Require(result.Applied, "valid edit applied");
        Require(result.Changed, "valid edit changed");
        Require(service.GetBlock(position).Value == 5, "query returns edited block");

        result = service.Apply(new BlockEdit(position, new BlockId(5)));
        Require(result.Applied, "same edit applied");
        Require(!result.Changed, "same edit unchanged");

        result = service.Apply(new BlockEdit(position, new BlockId(6)));
        Require(result.Applied, "replacement edit applied");
        Require(result.Changed, "replacement edit changed");
        Require(service.GetBlock(position).Value == 6, "query returns replacement block");

        result = service.Apply(new BlockEdit(position, BlockId.Air));
        Require(result.Applied, "air edit applied");
        Require(result.Changed, "air edit changed");
        Require(service.GetBlock(position) == BlockId.Air, "air edit removes override");

        Require(!service.Apply(new BlockEdit(new BlockPosition(0, ChunkConstants.WorldMinY - 1, 0), new BlockId(1))).Applied, "below world rejected");
        Require(service.Apply(new BlockEdit(new BlockPosition(0, ChunkConstants.WorldMinY, 0), new BlockId(1))).Applied, "bottom world block accepted");
        Require(service.Apply(new BlockEdit(new BlockPosition(0, ChunkConstants.WorldMaxYExclusive - 1, 0), new BlockId(1))).Applied, "top world block accepted");
        Require(!service.Apply(new BlockEdit(new BlockPosition(0, ChunkConstants.WorldMaxYExclusive, 0), new BlockId(1))).Applied, "height edge rejected");
        Require(!service.Apply(new BlockEdit(new BlockPosition(0, 0, 0), new BlockId(39))).Applied, "unknown block rejected");
    }

    private static void ValidateSupportRules()
    {
        var store = new BlockStore();
        var service = new BlockEditService(store, new BlockAuthorityRules());

        Require(!service.Apply(new BlockEdit(new BlockPosition(2, 1, 2), new BlockId(9))).Applied, "grass-supported block rejects missing grass");
        store.SetBlock(new BlockEdit(new BlockPosition(2, 0, 2), new BlockId(1)));

        var supportedPlant = service.Apply(new BlockEdit(new BlockPosition(2, 1, 2), new BlockId(9)));
        Require(supportedPlant.Applied, "grass-supported block applies above grass");
        Require(supportedPlant.Changed, "grass-supported block changes");
        Require(supportedPlant.Changes.Count == 1, "grass-supported block records one change");

        Require(!service.Apply(new BlockEdit(new BlockPosition(4, 1, 4), new BlockId(22))).Applied, "solid-base block rejects missing solid base");
        store.SetBlock(new BlockEdit(new BlockPosition(4, 0, 4), new BlockId(29)));

        var supportedTorch = service.Apply(new BlockEdit(new BlockPosition(4, 1, 4), new BlockId(22)));
        Require(supportedTorch.Applied, "solid-base block applies above solid base");
        Require(supportedTorch.Changed, "solid-base block changes");

        var removedSupport = service.Apply(new BlockEdit(new BlockPosition(4, 0, 4), BlockId.Air));
        Require(removedSupport.Applied, "support removal applies");
        Require(removedSupport.Changed, "support removal changes");
        Require(removedSupport.Changes.Count == 2, "support removal records cascade");
        Require(store.GetBlock(new BlockPosition(4, 1, 4)) == BlockId.Air, "support removal clears unsupported block above");
    }

    private static void ValidatePlayerSpawnAndWalkCollision()
    {
        var root = ResetProbeDirectory("player-collision");
        var store = new BlockStore();
        var rules = new BlockAuthorityRules();
        for (var z = -1; z <= 1; z++)
        for (var x = -1; x <= 1; x++)
        {
            store.SetBlock(new BlockEdit(new BlockPosition(x, 10, z), new BlockId(1)));
        }

        store.SetBlock(new BlockEdit(new BlockPosition(1, 11, 0), new BlockId(1)));
        store.SetBlock(new BlockEdit(new BlockPosition(1, 12, 0), new BlockId(1)));

        var controller = new PlayerController(new PlayerPersistence(root), store, rules);
        controller.AlignSpawnToSurface();
        var aligned = controller.Snapshot();
        Require(MathF.Abs(aligned.Y - (10.0f + NativePlayerSimulation.SpawnEyeHeight)) <= 0.001f, "player spawn aligns to solid surface");

        for (var index = 0; index < 24; index++)
        {
            var frame = new HostFrameSnapshot(
                new HostInputSnapshot(
                    HostInputSnapshot.VersionValue,
                    HostInputSnapshot.SizeValue,
                    flags: 0,
                    controller: 1,
                    moveX: 1.0f,
                    moveY: 0.0f,
                    moveZ: 0.0f,
                    cameraX: 0.0f,
                    cameraY: aligned.Y,
                    cameraZ: 0.0f,
                    cameraPitch: aligned.Pitch,
                    cameraYaw: 0.0f,
                    relativeMouse: 0),
                new HostFrameTimingSnapshot(
                    HostFrameTimingSnapshot.VersionValue,
                    HostFrameTimingSnapshot.SizeValue,
                    frameIndex: (ulong)(index + 1),
                    deltaSeconds: 1.0 / 20.0));
            var context = HostFrameContext.FromSnapshot(in frame);
            controller.Tick(in context);
        }

        var blocked = controller.Snapshot();
        Require(blocked.X <= 0.701f, "player walk collision blocks solid wall");
        Require(blocked.IsOnGround, "player gravity settles on ground");
        Require(MathF.Abs(blocked.VelocityX) <= 0.001f, "player blocked horizontal velocity clears");
    }

    private static void ValidateChunkMapping()
    {
        Require(BlockStore.ChunkPositionFor(new BlockPosition(0, 0, 0)) == new ChunkPosition(0, 0, 0), "origin chunk");
        Require(BlockStore.LocalPositionFor(new BlockPosition(0, 0, 0)) == new BlockPosition(0, 0, 0), "origin local");
        Require(BlockStore.ChunkPositionFor(new BlockPosition(31, 31, 31)) == new ChunkPosition(0, 0, 0), "edge chunk");
        Require(BlockStore.LocalPositionFor(new BlockPosition(31, 31, 31)) == new BlockPosition(31, 31, 31), "edge local");
        Require(BlockStore.ChunkPositionFor(new BlockPosition(0, 32, 0)) == new ChunkPosition(0, 1, 0), "vertical neighbor chunk");
        Require(BlockStore.LocalPositionFor(new BlockPosition(0, 32, 0)) == new BlockPosition(0, 0, 0), "vertical neighbor local");
        Require(
            BlockStore.ChunkPositionFor(new BlockPosition(0, ChunkConstants.WorldMinY, 0)) ==
            new ChunkPosition(0, ChunkConstants.WorldMinY / ChunkConstants.SectionHeight, 0),
            "bottom world chunk");
        Require(
            BlockStore.LocalPositionFor(new BlockPosition(0, ChunkConstants.WorldMinY, 0)) ==
            new BlockPosition(0, 0, 0),
            "bottom world local");
        Require(
            BlockStore.ChunkPositionFor(new BlockPosition(0, ChunkConstants.WorldMaxYExclusive - 1, 0)) ==
            new ChunkPosition(0, ChunkConstants.WorldMaxYExclusive / ChunkConstants.SectionHeight - 1, 0),
            "top world chunk");
        Require(
            BlockStore.LocalPositionFor(new BlockPosition(0, ChunkConstants.WorldMaxYExclusive - 1, 0)) ==
            new BlockPosition(0, ChunkConstants.SectionHeight - 1, 0),
            "top world local");
        Require(BlockStore.ChunkPositionFor(new BlockPosition(32, 0, 32)) == new ChunkPosition(1, 0, 1), "positive neighbor chunk");
        Require(BlockStore.LocalPositionFor(new BlockPosition(32, 0, 32)) == new BlockPosition(0, 0, 0), "positive neighbor local");
        Require(BlockStore.ChunkPositionFor(new BlockPosition(-1, 0, -1)) == new ChunkPosition(-1, 0, -1), "negative floor chunk");
        Require(BlockStore.LocalPositionFor(new BlockPosition(-1, 0, -1)) == new BlockPosition(31, 0, 31), "negative floor local");
    }

    private static void ValidateSnapshotOrder()
    {
        var store = new BlockStore();
        store.SetBlock(new BlockEdit(new BlockPosition(32, 1, 0), new BlockId(4)));
        store.SetBlock(new BlockEdit(new BlockPosition(-1, 1, 0), new BlockId(2)));
        store.SetBlock(new BlockEdit(new BlockPosition(0, 1, 0), new BlockId(3)));

        var snapshot = store.Snapshot();
        Require(snapshot.Count == 3, "snapshot count");
        Require(snapshot[0].Position == new BlockPosition(-1, 1, 0), "snapshot first negative chunk");
        Require(snapshot[1].Position == new BlockPosition(0, 1, 0), "snapshot second origin chunk");
        Require(snapshot[2].Position == new BlockPosition(32, 1, 0), "snapshot third positive chunk");
    }

    private static void ValidatePersistenceRoundTrip()
    {
        var root = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PROBE_DIR");
        if (string.IsNullOrWhiteSpace(root))
        {
            root = DefaultProbeRoot();
        }

        Directory.CreateDirectory(root);
        var path = Path.Combine(root, "world_blocks.json");
        if (File.Exists(path))
        {
            File.Delete(path);
        }

        var store = new BlockStore();
        store.SetBlock(new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(5)));
        store.SetBlock(new BlockEdit(new BlockPosition(-1, 1, 31), new BlockId(6)));
        WorldBlockOverrideFile.Save(path, WorldBlockOverrideFile.FromEdits(store.Snapshot()));

        Require(WorldBlockOverrideFile.TryLoad(path, out var file), "override file load");
        var loaded = new BlockStore();
        loaded.Load(file.ToEdits());
        Require(loaded.GetBlock(new BlockPosition(10, 1, 2)).Value == 5, "loaded positive edit");
        Require(loaded.GetBlock(new BlockPosition(-1, 1, 31)).Value == 6, "loaded negative edit");

        var json = File.ReadAllText(path);
        Require(json.Contains("\"version\"", StringComparison.Ordinal), "json version");
        Require(json.IndexOf("\"x\": -1", StringComparison.Ordinal) < json.IndexOf("\"x\": 10", StringComparison.Ordinal), "json sorted order");

        File.WriteAllText(path, json.Replace("\"version\": 1", "\"version\": 99", StringComparison.Ordinal));
        Require(!WorldBlockOverrideFile.TryLoad(path, out _), "unknown version rejected");
    }
}
