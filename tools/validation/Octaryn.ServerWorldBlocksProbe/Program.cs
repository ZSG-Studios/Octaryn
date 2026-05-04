using Octaryn.Basegame.Gameplay.Interaction;
using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

return ServerWorldBlocksProbe.Run();

internal static class ServerWorldBlocksProbe
{
    public static int Run()
    {
        ValidateWorldConstants();
        ValidateEditAndQuery();
        ValidateSupportRules();
        ValidatePlayerSpawnAndWalkCollision();
        ValidateChunkMapping();
        ValidateSnapshotOrder();
        ValidatePersistenceRoundTrip();
        ValidateCommandSink();
        ValidateClientCommandQueue();
        ValidateModuleCommandPath();
        ValidateSubmittedClientCommands();
        ValidateSnapshotDrain();
        ValidateActivatorPersistenceLifecycle();
        return 0;
    }

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

        Require(ServerBlockLimits.ChunkWidth == ChunkConstants.Width, "server chunk width mirrors shared contract");
        Require(ServerBlockLimits.ChunkDepth == ChunkConstants.Depth, "server chunk depth mirrors shared contract");
        Require(ServerBlockLimits.ChunkSectionHeight == ChunkConstants.SectionHeight, "server section height mirrors shared contract");
        Require(ServerBlockLimits.WorldHeight == ChunkConstants.WorldHeight, "server world height mirrors shared contract");
        Require(ServerBlockLimits.WorldMinY == ChunkConstants.WorldMinY, "server min y mirrors shared contract");
        Require(ServerBlockLimits.WorldMaxYExclusive == ChunkConstants.WorldMaxYExclusive, "server max y mirrors shared contract");
    }

    private static void ValidateEditAndQuery()
    {
        var store = new ServerBlockStore();
        var service = new ServerBlockEditService(store, new BasegameBlockAuthorityRules());
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
        var store = new ServerBlockStore();
        var service = new ServerBlockEditService(store, new BasegameBlockAuthorityRules());

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
        var store = new ServerBlockStore();
        var rules = new BasegameBlockAuthorityRules();
        for (var z = -1; z <= 1; z++)
        for (var x = -1; x <= 1; x++)
        {
            store.SetBlock(new BlockEdit(new BlockPosition(x, 10, z), new BlockId(1)));
        }

        store.SetBlock(new BlockEdit(new BlockPosition(1, 11, 0), new BlockId(1)));
        store.SetBlock(new BlockEdit(new BlockPosition(1, 12, 0), new BlockId(1)));

        var controller = new ServerPlayerController(new ServerPlayerPersistence(root), store, rules);
        controller.AlignSpawnToSurface();
        var aligned = controller.Snapshot();
        Require(MathF.Abs(aligned.Y - (10.0f + ServerPlayerCollision.SpawnEyeHeight)) <= 0.001f, "player spawn aligns to solid surface");

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
        Require(ServerBlockStore.ChunkPositionFor(new BlockPosition(0, 0, 0)) == new ChunkPosition(0, 0, 0), "origin chunk");
        Require(ServerBlockStore.LocalPositionFor(new BlockPosition(0, 0, 0)) == new BlockPosition(0, 0, 0), "origin local");
        Require(ServerBlockStore.ChunkPositionFor(new BlockPosition(31, 31, 31)) == new ChunkPosition(0, 0, 0), "edge chunk");
        Require(ServerBlockStore.LocalPositionFor(new BlockPosition(31, 31, 31)) == new BlockPosition(31, 31, 31), "edge local");
        Require(ServerBlockStore.ChunkPositionFor(new BlockPosition(0, 32, 0)) == new ChunkPosition(0, 1, 0), "vertical neighbor chunk");
        Require(ServerBlockStore.LocalPositionFor(new BlockPosition(0, 32, 0)) == new BlockPosition(0, 0, 0), "vertical neighbor local");
        Require(
            ServerBlockStore.ChunkPositionFor(new BlockPosition(0, ChunkConstants.WorldMinY, 0)) ==
            new ChunkPosition(0, ChunkConstants.WorldMinY / ChunkConstants.SectionHeight, 0),
            "bottom world chunk");
        Require(
            ServerBlockStore.LocalPositionFor(new BlockPosition(0, ChunkConstants.WorldMinY, 0)) ==
            new BlockPosition(0, 0, 0),
            "bottom world local");
        Require(
            ServerBlockStore.ChunkPositionFor(new BlockPosition(0, ChunkConstants.WorldMaxYExclusive - 1, 0)) ==
            new ChunkPosition(0, ChunkConstants.WorldMaxYExclusive / ChunkConstants.SectionHeight - 1, 0),
            "top world chunk");
        Require(
            ServerBlockStore.LocalPositionFor(new BlockPosition(0, ChunkConstants.WorldMaxYExclusive - 1, 0)) ==
            new BlockPosition(0, ChunkConstants.SectionHeight - 1, 0),
            "top world local");
        Require(ServerBlockStore.ChunkPositionFor(new BlockPosition(32, 0, 32)) == new ChunkPosition(1, 0, 1), "positive neighbor chunk");
        Require(ServerBlockStore.LocalPositionFor(new BlockPosition(32, 0, 32)) == new BlockPosition(0, 0, 0), "positive neighbor local");
        Require(ServerBlockStore.ChunkPositionFor(new BlockPosition(-1, 0, -1)) == new ChunkPosition(-1, 0, -1), "negative floor chunk");
        Require(ServerBlockStore.LocalPositionFor(new BlockPosition(-1, 0, -1)) == new BlockPosition(31, 0, 31), "negative floor local");
    }

    private static void ValidateSnapshotOrder()
    {
        var store = new ServerBlockStore();
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

        var store = new ServerBlockStore();
        store.SetBlock(new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(5)));
        store.SetBlock(new BlockEdit(new BlockPosition(-1, 1, 31), new BlockId(6)));
        WorldBlockOverrideFile.Save(path, WorldBlockOverrideFile.FromEdits(store.Snapshot()));

        Require(WorldBlockOverrideFile.TryLoad(path, out var file), "override file load");
        var loaded = new ServerBlockStore();
        loaded.Load(file.ToEdits());
        Require(loaded.GetBlock(new BlockPosition(10, 1, 2)).Value == 5, "loaded positive edit");
        Require(loaded.GetBlock(new BlockPosition(-1, 1, 31)).Value == 6, "loaded negative edit");

        var json = File.ReadAllText(path);
        Require(json.Contains("\"version\"", StringComparison.Ordinal), "json version");
        Require(json.IndexOf("\"x\": -1", StringComparison.Ordinal) < json.IndexOf("\"x\": 10", StringComparison.Ordinal), "json sorted order");

        File.WriteAllText(path, json.Replace("\"version\": 1", "\"version\": 99", StringComparison.Ordinal));
        Require(!WorldBlockOverrideFile.TryLoad(path, out _), "unknown version rejected");
    }

    private static void ValidateCommandSink()
    {
        var store = new ServerBlockStore();
        var rules = new BasegameBlockAuthorityRules();
        var sink = new ServerBlockCommandSink(new ServerBlockEditService(store, rules));

        Require(!sink.Enqueue(default), "default command rejected");
        Require(sink.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            A = 4,
            B = 5,
            C = 6,
            D = 7
        }), "set block command accepted");
        Require(store.GetBlock(new BlockPosition(4, 5, 6)).Value == 7, "set block command applied");

        Require(!sink.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            A = 4,
            B = ChunkConstants.WorldMaxYExclusive,
            C = 6,
            D = 7
        }), "height edge set block command rejected");

        var cascadingChanges = new ServerBlockChangeQueue();
        var cascadingStore = new ServerBlockStore();
        cascadingStore.SetBlock(new BlockEdit(new BlockPosition(9, 0, 9), new BlockId(29)));
        cascadingStore.SetBlock(new BlockEdit(new BlockPosition(9, 1, 9), new BlockId(22)));
        var cascadingSink = new ServerBlockCommandSink(new ServerBlockEditService(cascadingStore, rules), cascadingChanges);

        Require(cascadingSink.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            A = 9,
            B = 0,
            C = 9,
            D = 0
        }), "cascade set block command accepted");
        Require(cascadingStore.GetBlock(new BlockPosition(9, 1, 9)) == BlockId.Air, "cascade set block command clears unsupported block above");
        Require(cascadingChanges.PendingCount == 2, "cascade set block command records two changes");
    }

    private static void ValidateClientCommandQueue()
    {
        var store = new ServerBlockStore();
        var rules = new BasegameBlockAuthorityRules();
        var queue = new ServerClientBlockCommandQueue(
            new ServerBlockCommandSink(new ServerBlockEditService(store, rules)),
            rules);

        Require(queue.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            A = 3,
            B = 4,
            C = 5,
            D = 6
        }), "client block command queued");
        Require(queue.PendingCount == 1, "client block command pending");
        Require(queue.Drain() == 1, "client block command drained");
        Require(queue.PendingCount == 0, "client block command queue empty");
        Require(store.GetBlock(new BlockPosition(3, 4, 5)).Value == 6, "client block command applied");
        Require(!queue.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.None
        }), "unknown client command rejected");
        Require(!queue.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            A = 3,
            B = 4,
            C = 5,
            D = 15
        }), "non-placeable client block command rejected");
        Require(!queue.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            A = 3,
            B = 4,
            C = 5,
            D = 9
        }), "unsupported client block command rejected before queue");
        Require(queue.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            A = 3,
            B = 4,
            C = 5,
            D = 0
        }), "client block break command queued");
    }

    private static void ValidateModuleCommandPath()
    {
        var previousPath = UseProbePersistenceFile("module-command-path");
        try
        {
            using var activator = new ServerModuleActivator(new BlockEditRegistration());
            Require(activator.Activate(new RejectingCommandSink()) == 0, "module activation");

            var frame = new HostFrameSnapshot(
                new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue),
                new HostFrameTimingSnapshot(
                    HostFrameTimingSnapshot.VersionValue,
                    HostFrameTimingSnapshot.SizeValue,
                    frameIndex: 1,
                    deltaSeconds: 1.0 / 60.0));

            activator.Tick(in frame);
            Require(activator.GetBlock(new BlockPosition(8, 9, 10)).Value == 5, "module command applied to server world");
            Require(activator.SnapshotBlocks().Count == 1, "module command persisted in server snapshot");
        }
        finally
        {
            RestorePersistencePath(previousPath);
        }
    }

    private static unsafe void ValidateSubmittedClientCommands()
    {
        var previousPath = UseProbePersistenceFile("submitted-client-commands");
        try
        {
            using var activator = new ServerModuleActivator(new BlockEditRegistration());
            Require(activator.Activate(new RejectingCommandSink()) == 0, "submit command activator");

            var commands = new HostCommand[]
            {
                new()
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.SetBlock,
                    RequestId = 20,
                    A = -2,
                    B = 3,
                    C = 4,
                    D = 5
                }
            };

            fixed (HostCommand* commandPointer = commands)
            {
                Require(activator.SubmitClientCommands(commandPointer, (uint)commands.Length) == 0, "client command frame submitted");
            }

            Require(activator.PendingClientBlockCommandCount == 1, "submitted command pending");
            Require(activator.GetBlock(new BlockPosition(-2, 3, 4)) == BlockId.Air, "submitted command waits for tick");

            var frame = new HostFrameSnapshot(
                new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue),
                new HostFrameTimingSnapshot(
                    HostFrameTimingSnapshot.VersionValue,
                    HostFrameTimingSnapshot.SizeValue,
                    frameIndex: 2,
                    deltaSeconds: 1.0 / 60.0));

            activator.Tick(in frame);
            Require(activator.PendingClientBlockCommandCount == 0, "submitted command drained");
            Require(activator.GetBlock(new BlockPosition(-2, 3, 4)).Value == 5, "submitted command applied");

            var invalid = new HostCommand[]
            {
                new()
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.None
                }
            };

            fixed (HostCommand* invalidPointer = invalid)
            {
                Require(activator.SubmitClientCommands(invalidPointer, (uint)invalid.Length) != 0, "invalid submitted command rejected");
            }

            var unsupported = new HostCommand[]
            {
                new()
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.SetBlock,
                    A = 21,
                    B = 3,
                    C = 4,
                    D = 9
                }
            };

            fixed (HostCommand* unsupportedPointer = unsupported)
            {
                Require(activator.SubmitClientCommands(unsupportedPointer, (uint)unsupported.Length) == -2, "unsupported submitted command rejected");
            }
            Require(activator.PendingClientBlockCommandCount == 0, "unsupported submitted command leaves no pending commands");

            var nonPlaceable = new HostCommand[]
            {
                new()
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.SetBlock,
                    A = 21,
                    B = 3,
                    C = 4,
                    D = 15
                }
            };

            fixed (HostCommand* nonPlaceablePointer = nonPlaceable)
            {
                Require(activator.SubmitClientCommands(nonPlaceablePointer, (uint)nonPlaceable.Length) == -2, "non-placeable submitted command rejected");
            }
            Require(activator.PendingClientBlockCommandCount == 0, "non-placeable submitted command leaves no pending commands");

            var mixed = new HostCommand[]
            {
                new()
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.SetBlock,
                    A = 20,
                    B = 3,
                    C = 4,
                    D = 5
                },
                new()
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.None
                }
            };

            fixed (HostCommand* mixedPointer = mixed)
            {
                Require(activator.SubmitClientCommands(mixedPointer, (uint)mixed.Length) == -2, "mixed submitted frame rejected");
            }

            Require(activator.PendingClientBlockCommandCount == 0, "mixed submitted frame leaves no pending commands");
            activator.Tick(in frame);
            Require(activator.GetBlock(new BlockPosition(20, 3, 4)) == BlockId.Air, "mixed submitted frame did not partially apply");

            Require(activator.SubmitClientCommands(null, 1) == -1, "missing submitted command buffer rejected");

            var emptyFrame = new ClientCommandFrame(0, tickId: 3, commandsAddress: 0);
            Require(emptyFrame.CommandCount == 0 && emptyFrame.CommandsAddress == 0, "empty client command frame constructible");
        }
        finally
        {
            RestorePersistencePath(previousPath);
        }
    }

    private static unsafe void ValidateSnapshotDrain()
    {
        var previousPath = UseProbePersistenceFile("snapshot-drain");
        try
        {
            using var activator = new ServerModuleActivator(new BlockEditRegistration());
            Require(activator.Activate(new RejectingCommandSink()) == 0, "snapshot drain activator");
            Require(activator.DrainServerSnapshots(null) == -1, "server snapshot drain rejects invalid header");

            var command = new HostCommand
            {
                Version = HostCommand.VersionValue,
                Size = HostCommand.SizeValue,
                Kind = HostCommandKind.SetBlock,
                A = -4,
                B = 5,
                C = 6,
                D = 7
            };

            Require(activator.SubmitClientCommands(&command, 1) == 0, "snapshot drain submitted block edit");
            var frame = new HostFrameSnapshot(
                new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue),
                new HostFrameTimingSnapshot(
                    HostFrameTimingSnapshot.VersionValue,
                    HostFrameTimingSnapshot.SizeValue,
                    frameIndex: 3,
                    deltaSeconds: 1.0 / 60.0));
            activator.Tick(in frame);

            Require(activator.PendingBlockChangeCount == 2, "server snapshot drain reports block change count");

            var changes = new ReplicationChange[4];
            fixed (ReplicationChange* changesPointer = changes)
            {
                var header = new ServerSnapshotHeader(
                    replicationCount: 0,
                    changeCount: (uint)changes.Length,
                    tickId: 0,
                    replicationIdsAddress: 0,
                    changesAddress: (ulong)changesPointer);
                Require(activator.DrainServerSnapshots(&header) == 0, "server snapshot drain includes submitted block edit");
                Require(header.ChangeCount == 2, "server snapshot drain output count");
                Require(header.ReplicationCount == 0, "server snapshot drain no replication ids");
                Require(changes[0].ChangeKind == ServerBlockChangeQueue.BlockEditChangeKind, "server snapshot drain block change kind");
                Require(UnpackLow(changes[0].Payload0) == -4, "server snapshot drain first x");
                Require(UnpackHigh(changes[0].Payload0) == 5, "server snapshot drain first y");
                Require(UnpackLow(changes[0].Payload1) == 6, "server snapshot drain first z");
                Require((ushort)(changes[0].Payload1 >> 32) == 7, "server snapshot drain first block");
                Require(changes[1].ChangeKind == ServerBlockChangeQueue.BlockEditChangeKind, "server snapshot drain module block change kind");
                Require(activator.PendingBlockChangeCount == 0, "server snapshot drain clears pending block changes");

                Require(activator.DrainServerSnapshots(&header) == 0, "server snapshot drain empty");
                Require(header.ChangeCount == 0, "server snapshot drain empty count");
            }
        }
        finally
        {
            RestorePersistencePath(previousPath);
        }
    }

    private static unsafe void ValidateActivatorPersistenceLifecycle()
    {
        var root = ResetProbeDirectory("activator-persistence");
        var path = Path.Combine(root, "world_blocks.json");
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        SetPersistencePath(path);

        try
        {
            using (var activator = new ServerModuleActivator(new BlockEditRegistration()))
            {
                Require(activator.Activate(new RejectingCommandSink()) == 0, "persistence activator activation");
                activator.Tick(Frame(10));
            }

            Require(File.Exists(path), "persistence file created on dispose");

            using (var loaded = new ServerModuleActivator(new BlockEditRegistration()))
            {
                Require(loaded.GetBlock(new BlockPosition(8, 9, 10)).Value == 5, "persistence loaded module edit");
            }

            WorldBlockOverrideFile.Save(path, new WorldBlockOverrideFile
            {
                Blocks =
                [
                    new WorldBlockOverrideRecord(9, 0, 9, 29),
                    new WorldBlockOverrideRecord(9, 1, 9, 22)
                ]
            });

            using (var cascade = new ServerModuleActivator(new BlockEditRegistration()))
            {
                Require(cascade.GetBlock(new BlockPosition(9, 0, 9)).Value == 29, "persistence loaded support block");
                Require(cascade.GetBlock(new BlockPosition(9, 1, 9)).Value == 22, "persistence loaded supported block");

                var command = new HostCommand
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.SetBlock,
                    A = 9,
                    B = 0,
                    C = 9,
                    D = 0
                };
                Require(cascade.SubmitClientCommands(&command, 1) == 0, "persistence cascade break submitted");
                Require(cascade.Activate(new RejectingCommandSink()) == 0, "persistence cascade activator activation");
                cascade.Tick(Frame(11));
            }

            Require(WorldBlockOverrideFile.TryLoad(path, out var saved), "persistence cascade file reload");
            var savedStore = new ServerBlockStore();
            savedStore.Load(saved.ToEdits());
            Require(savedStore.GetBlock(new BlockPosition(9, 0, 9)) == BlockId.Air, "persistence saved removed support");
            Require(savedStore.GetBlock(new BlockPosition(9, 1, 9)) == BlockId.Air, "persistence saved cascade removal");
        }
        finally
        {
            RestorePersistencePath(previousPath);
        }
    }

    private static HostFrameSnapshot Frame(ulong frameIndex)
    {
        return new HostFrameSnapshot(
            new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue),
            new HostFrameTimingSnapshot(
                HostFrameTimingSnapshot.VersionValue,
                HostFrameTimingSnapshot.SizeValue,
                frameIndex,
                deltaSeconds: 1.0 / 60.0));
    }

    private static string ResetProbeDirectory(string name)
    {
        var root = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PROBE_DIR");
        if (string.IsNullOrWhiteSpace(root))
        {
            root = DefaultProbeRoot();
        }

        var directory = Path.Combine(root, name);
        if (Directory.Exists(directory))
        {
            Directory.Delete(directory, recursive: true);
        }

        Directory.CreateDirectory(directory);
        return directory;
    }

    private static string? UseProbePersistenceFile(string name)
    {
        var directory = ResetProbeDirectory(name);
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        SetPersistencePath(Path.Combine(directory, "world_blocks.json"));
        return previousPath;
    }

    private static void SetPersistencePath(string path)
    {
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", path);
    }

    private static void RestorePersistencePath(string? previousPath)
    {
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", previousPath);
    }

    private static string DefaultProbeRoot()
    {
        var presetName = Environment.GetEnvironmentVariable("OctarynBuildPresetName");
        if (string.IsNullOrWhiteSpace(presetName))
        {
            presetName = "debug-linux";
        }

        return Path.Combine("build", presetName, "server", "validation", "server-world-blocks-probe");
    }

    private static int UnpackLow(ulong value)
    {
        return unchecked((int)(uint)value);
    }

    private static int UnpackHigh(ulong value)
    {
        return unchecked((int)(uint)(value >> 32));
    }

    private static void Require(bool condition, string label)
    {
        if (!condition)
        {
            throw new InvalidOperationException($"Server world blocks probe failed: {label}.");
        }
    }

    private sealed class BlockEditRegistration : IGameModuleRegistration, IBlockAuthorityRulesProvider
    {
        public IBlockAuthorityRules BlockAuthorityRules { get; } = new BasegameBlockAuthorityRules();

        public GameModuleManifest Manifest { get; } = new(
            ModuleId: "octaryn.probe.server_world_blocks",
            DisplayName: "Octaryn Server World Blocks Probe",
            Version: "0.1.0",
            OctarynApiVersion: "0.1.0",
            RequiredCapabilities:
            [
                ModuleCapabilityIds.GameplayRules,
                ModuleCapabilityIds.WorldBlockEdits
            ],
            RequestedHostApis:
            [
                HostApiIds.Commands,
                HostApiIds.Frame
            ],
            RequestedRuntimePackages: [],
            RequestedBuildPackages: [],
            RequestedFrameworkApiGroups: [],
            ModuleDependencies: [],
            ContentDeclarations: [],
            AssetDeclarations: [],
            Schedule: new GameModuleScheduleDeclaration(
            [
                new ScheduledSystemDeclaration(
                    SystemId: "octaryn.probe.server_world_blocks.tick",
                    Phase: HostWorkPhase.Gameplay,
                    FrameOrTickOwner: HostScheduleIds.FrameOrTickOwner,
                    Reads:
                    [
                        new ScheduledResourceAccess(HostApiIds.Frame, ScheduledAccessMode.Read)
                    ],
                    Writes:
                    [
                        new ScheduledResourceAccess(HostApiIds.Commands, ScheduledAccessMode.Write)
                    ],
                    RunsAfter: [],
                    RunsBefore: [],
                    Flags: HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier,
                    CommitBarrier: HostScheduleIds.FrameOrTickEndBarrier)
            ]),
            Compatibility: new GameModuleCompatibility(
                MinimumHostApiVersion: "0.1.0",
                MaximumHostApiVersion: "0.1.0",
                SaveCompatibilityId: "octaryn.probe.server_world_blocks.save.v0",
                SupportsMultiplayer: false));

        public IGameModuleInstance CreateInstance(ModuleHostContext context)
        {
            return new BlockEditModule(context);
        }
    }

    private sealed class BlockEditModule(ModuleHostContext context) : IGameModuleInstance
    {
        public void Tick(in ModuleFrameContext frame)
        {
            _ = frame;
            if (!context.Commands.TryRequest(ModuleCommandRequest.SetBlock(
                new BlockEdit(new BlockPosition(8, 9, 10), new BlockId(5)),
                requestId: 12)))
            {
                throw new InvalidOperationException("server world blocks probe command was rejected.");
            }
        }

        public void Dispose()
        {
        }
    }

    private sealed class RejectingCommandSink : IHostCommandSink
    {
        public bool Enqueue(HostCommand command)
        {
            _ = command;
            return false;
        }
    }
}
