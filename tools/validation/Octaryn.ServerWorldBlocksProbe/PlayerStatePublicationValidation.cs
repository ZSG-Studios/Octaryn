using Octaryn.Server;
using Octaryn.Server.Modules;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Chunks;

internal static partial class ServerWorldBlocksProbe
{
    private static void ValidatePlayerStatePublicationClock()
    {
        var clock = new PlayerStatePublicationClock();
        var writes = 0;
        for (var iteration = 0; iteration < 625; ++iteration)
        {
            var now = iteration * 0.016;
            if (!clock.ShouldPublish(now, (ulong)iteration)) continue;
            clock.Published(now, (ulong)iteration);
            ++writes;
            Require(!clock.ShouldPublish(now, (ulong)iteration), "same state must not publish twice");
        }
        Require(writes is >= 599 and <= 600,
            "16ms authority loop must retain 60Hz publication phase instead of halving cadence");

        clock = new PlayerStatePublicationClock();
        Require(clock.ShouldPublish(0, 0), "startup baseline due");
        clock.Published(0, 0);
        Require(!clock.ShouldPublish(0.001, 1), "early state waits for deadline");
        Require(clock.ShouldPublish(0.02, 1), "deadline crossed");
        // A failed writer does not call Published: the next attempt remains due.
        Require(clock.ShouldPublish(0.021, 1), "failed publication remains retryable");
        clock.Published(0.021, 1);
        Require(clock.ShouldPublish(0.034, 2), "late write must not shift next deadline to 0.0377");
        clock.Published(0.034, 2);
        Require(clock.ShouldPublish(2, 100), "long stall publishes newest state once");
        clock.Published(2, 100);
        Require(!clock.ShouldPublish(2, 101), "long stall does not produce catch-up writes");
        Require(!clock.ShouldPublish(3, 100), "unchanged source tick remains suppressed");
        Require(!clock.ShouldPublish(double.NaN, 101), "nonfinite clock rejected");
        ValidateTrackedProcessTick();
        Console.WriteLine($"player_publication_clock=passed writes_10s={writes} cadence=60hz retry=passed stall=passed");
    }

    private static unsafe void ValidateTrackedProcessTick()
    {
        var previous = UseProbePersistenceFile("player-source-clock");
        var tracker = NativeBlockStoreLibrary.ChunkStreamWriteTrackerCreate();
        try
        {
            Require(tracker != IntPtr.Zero, "source clock native planner allocation");
            using var authority = new ModuleActivator(new BlockEditRegistration(), BlockPublicationMode.ProcessSnapshots);
            Require(authority.Activate(new RejectingCommandSink()) == 0, "source clock authority activation");
            var intent = new NativeChunkViewIntent(1, 1, 0, 0, 0, 0, 0, 0, 0);
            var plan = default(NativeChunkStreamProcessStagePlan);
            Require(NativeBlockStoreLibrary.ChunkStreamPlanProcessStage(tracker, &intent, 0, 1, 1, &plan) == 0 &&
                plan.Tick.ShouldTick == 1 && plan.Tick.UseHostOnlyTick == 1 && plan.Tick.UseDefaultFrame == 1,
                "actual metadata command planner chooses default host-only authority tick");
            var before = ChunkStreamProcessBridge.PlayerSourceClock;
            var worldBefore = authority.SnapshotWorldTime().TotalWorldSeconds;
            var frame = Frame(1);
            Require(ChunkStreamProcessBridge.ExecuteTrackedPlayerTick(authority, in frame, plan.Tick) == 0,
                "actual process bridge executes host-only authority tick");
            var after = ChunkStreamProcessBridge.PlayerSourceClock;
            Require(after.Tick == before.Tick + 1 && Math.Abs(after.Seconds - before.Seconds - 1.0 / 60.0) < 1e-10 &&
                authority.SnapshotWorldTime().TotalWorldSeconds > worldBefore,
                "host-only authority movement and world time must advance published source identity");
            Require(ChunkStreamProcessBridge.ExecuteTrackedPlayerTick(authority, in frame, default) == 0 &&
                ChunkStreamProcessBridge.PlayerSourceClock == after, "skipped process tick preserves source clock");
            Console.WriteLine("player_source_clock=passed actual_host_only=passed idle=passed");
        }
        finally
        {
            NativeBlockStoreLibrary.ChunkStreamWriteTrackerDestroy(tracker);
            RestorePersistencePath(previous);
        }
    }
}
