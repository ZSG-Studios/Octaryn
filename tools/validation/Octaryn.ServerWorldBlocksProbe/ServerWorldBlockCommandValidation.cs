using Octaryn.Basegame.Gameplay.Interaction;
using Octaryn.Server;
using Octaryn.Server.Modules;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    private static void ValidateCommandSink()
    {
        var store = new BlockStore();
        var rules = new BlockAuthorityRules();
        var sink = new BlockCommandSink(new BlockEditService(store, rules));

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

        var interactionStore = new BlockStore();
        interactionStore.SetBlock(new BlockEdit(new BlockPosition(0, 0, 0), new BlockId(1)));
        var interactionSink = new BlockCommandSink(new BlockEditService(interactionStore, rules));
        Require(interactionSink.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            Flags = HostCommand.ClientInteractionFlag,
            A = 1,
            B = 0,
            C = 0,
            D = 5,
            X = 0.5f,
            Y = 0.5f,
            Z = 0.5f,
            X2 = 0.0f,
            Y2 = 0.0f,
            Z2 = 0.0f
        }), "adjacent client interaction place accepted");
        Require(interactionStore.GetBlock(new BlockPosition(1, 0, 0)).Value == 5, "adjacent client interaction place applied");

        Require(!interactionSink.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            Flags = HostCommand.ClientInteractionFlag,
            A = 2,
            B = 0,
            C = 0,
            D = 5,
            X = 20.0f,
            Y = 0.5f,
            Z = 0.5f,
            X2 = 0.0f,
            Y2 = 0.0f,
            Z2 = 0.0f
        }), "far client interaction rejected");

        Require(interactionSink.Enqueue(new HostCommand
        {
            Version = HostCommand.VersionValue,
            Size = HostCommand.SizeValue,
            Kind = HostCommandKind.SetBlock,
            Flags = HostCommand.ClientInteractionFlag,
            A = 0,
            B = 0,
            C = 0,
            D = 0,
            X = 0.5f,
            Y = 0.5f,
            Z = 0.5f,
            X2 = 0.0f,
            Y2 = 0.0f,
            Z2 = 0.0f
        }), "client interaction break accepted");
        Require(interactionStore.GetBlock(new BlockPosition(0, 0, 0)) == BlockId.Air, "client interaction break applied");

        var cascadingChanges = new BlockChangeQueue();
        var cascadingStore = new BlockStore();
        cascadingStore.SetBlock(new BlockEdit(new BlockPosition(9, 0, 9), new BlockId(29)));
        cascadingStore.SetBlock(new BlockEdit(new BlockPosition(9, 1, 9), new BlockId(22)));
        var cascadingSink = new BlockCommandSink(new BlockEditService(cascadingStore, rules), cascadingChanges);

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

    private static unsafe void ValidateClientCommandQueue()
    {
        var store = new BlockStore();
        var rules = new BlockAuthorityRules();
        var queue = new ClientBlockCommandQueue(
            new BlockEditService(store, rules),
            rules);

        var accepted = new HostCommand[]
        {
            new()
            {
                Version = HostCommand.VersionValue,
                Size = HostCommand.SizeValue,
                Kind = HostCommandKind.SetBlock,
                A = 3,
                B = 4,
                C = 5,
                D = 6
            }
        };
        fixed (HostCommand* acceptedPointer = accepted)
        {
            Require(queue.Submit(acceptedPointer, (uint)accepted.Length).Result == 0, "client block command queued");
        }
        Require(queue.PendingCount == 1, "client block command pending");
        Require(queue.Drain() == 1, "client block command drained");
        Require(queue.PendingCount == 0, "client block command queue empty");
        Require(store.GetBlock(new BlockPosition(3, 4, 5)).Value == 6, "client block command applied");
        var unknown = new HostCommand[]
        {
            new()
            {
                Version = HostCommand.VersionValue,
                Size = HostCommand.SizeValue,
                Kind = HostCommandKind.None
            }
        };
        fixed (HostCommand* unknownPointer = unknown)
        {
            Require(queue.Submit(unknownPointer, (uint)unknown.Length).Result == -2, "unknown client command rejected");
        }
        var nonPlaceable = new HostCommand[]
        {
            new()
            {
                Version = HostCommand.VersionValue,
                Size = HostCommand.SizeValue,
                Kind = HostCommandKind.SetBlock,
                A = 3,
                B = 4,
                C = 5,
                D = 15
            }
        };
        fixed (HostCommand* nonPlaceablePointer = nonPlaceable)
        {
            Require(queue.Submit(nonPlaceablePointer, (uint)nonPlaceable.Length).Result == -2, "non-placeable client block command rejected");
        }
        var unsupported = new HostCommand[]
        {
            new()
            {
                Version = HostCommand.VersionValue,
                Size = HostCommand.SizeValue,
                Kind = HostCommandKind.SetBlock,
                A = 3,
                B = 4,
                C = 5,
                D = 9
            }
        };
        fixed (HostCommand* unsupportedPointer = unsupported)
        {
            Require(queue.Submit(unsupportedPointer, (uint)unsupported.Length).Result == -2, "unsupported client block command rejected before queue");
        }
        var breakCommand = new HostCommand[]
        {
            new()
            {
                Version = HostCommand.VersionValue,
                Size = HostCommand.SizeValue,
                Kind = HostCommandKind.SetBlock,
                A = 3,
                B = 4,
                C = 5,
                D = 0
            }
        };
        fixed (HostCommand* breakCommandPointer = breakCommand)
        {
            Require(queue.Submit(breakCommandPointer, (uint)breakCommand.Length).Result == 0, "client block break command queued");
        }
    }

    private static void ValidateModuleCommandPath()
    {
        var previousPath = UseProbePersistenceFile("module-command-path");
        try
        {
            using var activator = new ModuleActivator(new BlockEditRegistration());
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
            using var activator = new ModuleActivator(new BlockEditRegistration());
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

            var playerCollisionPlace = new HostCommand[]
            {
                new()
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.SetBlock,
                    A = 0,
                    B = 80,
                    C = 0,
                    D = 5
                }
            };

            fixed (HostCommand* playerCollisionPlacePointer = playerCollisionPlace)
            {
                Require(activator.SubmitClientCommands(playerCollisionPlacePointer, (uint)playerCollisionPlace.Length) == -2, "player collision submitted place rejected");
            }
            Require(activator.PendingClientBlockCommandCount == 0, "player collision submitted place leaves no pending commands");

            var invalidInteraction = new HostCommand[]
            {
                new()
                {
                    Version = HostCommand.VersionValue,
                    Size = HostCommand.SizeValue,
                    Kind = HostCommandKind.SetBlock,
                    Flags = HostCommand.ClientInteractionFlag,
                    A = 1,
                    B = 0,
                    C = 0,
                    D = 5,
                    X = 0.5f,
                    Y = 0.5f,
                    Z = 0.5f,
                    X2 = 0.0f,
                    Y2 = 0.0f,
                    Z2 = 0.0f
                }
            };

            fixed (HostCommand* invalidInteractionPointer = invalidInteraction)
            {
                Require(activator.SubmitClientCommands(invalidInteractionPointer, (uint)invalidInteraction.Length) == -2, "invalid interaction submitted command rejected");
            }
            Require(activator.PendingClientBlockCommandCount == 0, "invalid interaction submitted command leaves no pending commands");

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
}
