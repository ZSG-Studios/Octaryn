using Octaryn.Server;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking.Remote;
using Octaryn.Shared.World;

internal static class PlayerSimulationQualification
{
    public static int Run(string directory)
    {
        Directory.CreateDirectory(directory);
        using var blocks = new BlockStore();
        var rules = new GroundRules();
        using var world = new PlayerSimulationWorld(blocks, rules,
            position => position.Y == 0 ? new BlockId(1) : BlockId.Air);
        var initial = NativePlayerSimulation.DefaultState() with
        {
            X = 0, Y = NativePlayerSimulation.SpawnEyeHeight, Z = 0,
            Pitch = 0, Yaw = 0, IsOnGround = true
        };
        var first = world.Add(1, initial);
        var second = world.Add(2, initial with { Z = 8 });
        var firstStart = world.Snapshot(first);
        var secondStart = world.Snapshot(second);
        world.Queue(first, Frame(1, 1, 0));
        world.Queue(second, Frame(1, -1, 0));
        ExpectThrows(() => world.Queue(first, Frame(2, 0, 0)), "duplicate pending command rejected");
        world.StepQueued();
        var a = world.Snapshot(first);
        var b = world.Snapshot(second);
        Require(a.X > firstStart.X && b.X < secondStart.X && b.Z == secondStart.Z,
            "distinct accepted inputs move two independent native bodies");
        world.StepQueued();
        Require(world.Snapshot(first) == a && world.Snapshot(second) == b, "batch cannot replay drained actors");
        world.Queue(second, Frame(2, -1, 0));
        var returned = world.StepOne(first, Frame(2, 1, 0), out _);
        Require(world.Snapshot(first) == returned && world.Snapshot(second) == b,
            "step-one publishes component and does not drain another actor");
        world.StepQueued();
        Require(world.Snapshot(first) == returned && world.Snapshot(second).X < b.X,
            "remaining batch steps only queued actor once");
        b = world.Snapshot(second);
        world.Remove(first);
        first = world.Add(1, initial with { X = 32 });
        Require(world.Snapshot(second) == b && world.Snapshot(first).X == 32,
            "recreating one body preserves other body and complete component state");
        ExpectThrows(() => world.Snapshot(new PlayerSimulationIdentity(1, first.Generation - 2)),
            "stale generation cannot address recreated entity");
        world.StepOne(first, Frame(2, 1, 0), out _);
        var seamStartX = world.Snapshot(first).X;
        world.SaveIfDue(first, directory, 0, true);
        world.SaveIfDue(second, directory, 0, true);
        Require(NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(directory, 1, out var savedA)
            && NativeWorldPersistenceLibrary.TryReadPlayerDirectoryEntry(directory, 2, out var savedB)
            && savedA.X == world.Snapshot(first).X && savedB.X == b.X && savedB.Z == b.Z,
            "identity-keyed persistence serializes authoritative component state");
        Console.WriteLine("player_ecs independent_bodies=pass batch_once=pass step_one=pass generations=pass persistence=pass");

        // Cross many voxel seams and compare the ECS system against the retained native character path.
        using var referenceBlocks = new BlockStore();
        var native = new NativePlayerSimulation(referenceBlocks, rules,
            position => position.Y == 0 ? new BlockId(1) : BlockId.Air);
        var reference = NativePlayerSimulation.CreateSession(world.Snapshot(first), false);
        var jumped = false;
        var landed = false;
        var previousY = world.Snapshot(first).Y;
        var seamTravel = 0f;
        try
        {
            for (ulong tick = 3; tick < 303; tick++)
            {
                var jump = tick >= 100 && tick <= 105;
                var frame = Frame(tick, 1, jump ? HostInputSnapshot.JumpFlag : 0);
                var expected = native.Step(reference, frame.Input, frame.DeltaSeconds, out _);
                var actual = world.StepOne(first, frame, out _);
                Require(actual == expected && world.Snapshot(first) == actual,
                    "complete state matches native Jolt path including jump edge");
                Require(world.Snapshot(second) == b, "idle second actor remains unchanged throughout first actor motion");
                if (tick < 100)
                {
                    Require(MathF.Abs(actual.Y - previousY) < 0.01f, "ground seams preserve eye height");
                    seamTravel = actual.X - seamStartX;
                }
                if (jump && actual.VelocityY > 0 && actual.JumpHeld) jumped = true;
                if (tick > 110 && jumped && actual.IsOnGround && !actual.JumpHeld) landed = true;
                previousY = actual.Y;
            }
        }
        finally { NativePlayerSimulation.DestroySession(reference); }
        Require(seamTravel > 4 && jumped && landed, "Jolt seam traversal, press/release and landing exercised");
        Console.WriteLine($"player_ecs native_parity_steps=300 seam_travel={seamTravel:F3} jump=pass landing=pass");
        QualifyBudget(world, first);
        QualifyControllerLifetime(world, directory);
        return 0;
    }

    private static void QualifyControllerLifetime(PlayerSimulationWorld world, string directory)
    {
        using var survivor = new PlayerController(directory, world, 4);
        var survivorStart = survivor.Snapshot();
        PlayerState saved;
        using (var controller = new PlayerController(directory, world, 3))
        {
            var start = controller.Snapshot();
            controller.Tick(Frame(1, 1, HostInputSnapshot.FlyModeFlag));
            saved = controller.Snapshot();
            Require(saved.X > start.X && survivor.Snapshot() == survivorStart,
                "controller tick publishes component state without stepping sibling");
        }
        using var recreated = new PlayerController(directory, world, 3);
        Require(Math.Abs(recreated.Snapshot().X - saved.X) < 0.00001f && survivor.Snapshot() == survivorStart,
            "controller disposal saves/removes only its actor; recreation loads its own identity");
        survivor.Tick(Frame(1, -1, HostInputSnapshot.FlyModeFlag));
        Require(survivor.Snapshot().X < survivorStart.X, "sibling native body remains usable after disposal/recreation");
        Console.WriteLine("player_ecs controller_publication=pass dispose_recreate=pass sibling_lifetime=pass");
    }

    private static void QualifyBudget(PlayerSimulationWorld world, PlayerSimulationIdentity player)
    {
        var time = 1.0;
        ulong ticks = 0;
        var queue = new PlayerCommandQueue(() => time);
        var commands = Enumerable.Range(1, 64)
            .Select(i => new PlayerCommand((ulong)i, HostInputSnapshot.FlyModeFlag, 1, 1, 0, 0, 0, 0, 1)).ToArray();
        queue.Accept(commands);
        int Drain()
        {
            queue.Accrue();
            var count = 0;
            while (count < 8 && queue.TrySelect(ticks + 1, out var snapshot))
            {
                var frame = HostFrameContext.FromSnapshot(snapshot);
                world.StepOne(player, frame, out _);
                queue.Commit(snapshot);
                ticks++;
                count++;
            }
            return count;
        }
        var before = world.Snapshot(player);
        Require(Drain() == 0 && world.Snapshot(player) == before, "ECS actor cannot acquire client-granted time");
        time += 0.1;
        Require(Drain() == 6 && queue.Acknowledged == 6, "fixed wall budget grants exactly six ECS steps");
        var moved = world.Snapshot(player);
        for (var i = 0; i < 100; i++) { queue.Accept(commands); Require(Drain() == 0, "duplicate flood grants no ECS step"); }
        Require(world.Snapshot(player) == moved && moved.X > before.X, "authoritative ECS motion respects command budget");
        Console.WriteLine("player_ecs fixed_budget_steps=6 duplicate_flood=100 no_client_credit=pass");
    }

    private static HostFrameContext Frame(ulong tick, float moveX, uint flags) => new(1.0 / 60, tick,
        new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue,
            flags, 1, moveX, 0, 0, 0, 0, 0, 0, 0, 0));

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    private static void ExpectThrows(Action action, string message)
    {
        try { action(); }
        catch (InvalidOperationException) { return; }
        throw new InvalidOperationException(message);
    }

    private sealed class GroundRules : IBlockAuthorityRules
    {
        public bool IsKnownBlock(BlockId block) => true;
        public bool CanApplyEdit(BlockEdit edit, BlockId belowBlock) => true;
        public bool CanStaySupported(BlockId block, BlockPosition position, BlockId belowBlock) => true;
        public bool IsClientPlaceable(BlockId block) => block.Value == 1;
        public bool IsSolidBlock(BlockId block) => block.Value == 1;
    }
}
