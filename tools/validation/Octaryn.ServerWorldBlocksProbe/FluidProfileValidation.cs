using System.Diagnostics;
using Octaryn.Basegame.Module;
using Octaryn.Server.Modules;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    private static void ValidateBasegameFluidProfile()
    {
        var previous = UseProbePersistenceFile("basegame-fluid-profile");
        var console = Console.Out;
        var milliseconds = new List<double>();
        var fluidMilliseconds = new List<double>();
        uint changed = 0, maxPending = 0, maxReads = 0, budgetStops = 0;
        try
        {
            Console.SetOut(TextWriter.Null);
            var registration = new ModuleRegistration();
            using var authority = new ModuleActivator(registration, BlockPublicationMode.ProcessSnapshots);
            Require(authority.Activate(new RejectingCommandSink()) == 0, "real basegame fluid profile activation");
            authority.SetFluidRegion(0, 0, 4);
            var source = new BlockPosition(8, 160, 8);
            Require(authority.SubmitClientCommands(new[] { FlowCommand(source, registration.FluidRules.WaterLevels[0]) }) == 0,
                "basegame generated-world fluid source accepted");
            for (var frame = 0; frame < 150; frame++)
            {
                var started = Stopwatch.GetTimestamp();
                authority.Tick(FlowFrame((ulong)frame + 1, 1.0 / 60.0));
                var elapsed = Stopwatch.GetElapsedTime(started).TotalMilliseconds;
                if (frame >= 30) { milliseconds.Add(elapsed); fluidMilliseconds.Add(authority.FluidStepMilliseconds); }
                var report = authority.FluidReport;
                changed += report.Changed;
                maxPending = Math.Max(maxPending, report.Pending);
                maxReads = Math.Max(maxReads, report.Reads);
                budgetStops += report.BudgetExhausted != 0 ? 1u : 0u;
                Require(report.Pending <= 8192 && report.Reads <= 65536 && report.Evaluations <= 256 &&
                    report.ApplyAttempts <= 128 && report.RepairSamples <= 4096, "real basegame fluid work remains bounded");
            }
            Require(changed > 0 && authority.GetBlock(new BlockPosition(8, 159, 8)) != BlockId.Air,
                "real generated-world fluid service applies source flow");
            Require(authority.PendingBlockChangeCount == 0, "real standalone mode retains no unused deltas");
        }
        finally
        {
            Console.SetOut(console);
            RestorePersistencePath(previous);
        }
        milliseconds.Sort();
        fluidMilliseconds.Sort();
        Console.WriteLine(FormattableString.Invariant($"basegame_fluid_profile=passed samples={milliseconds.Count} authority_tick_mean_ms={milliseconds.Average():F3} p95_ms={milliseconds[(int)(milliseconds.Count * 0.95)]:F3} max_ms={milliseconds[^1]:F3} fluid_step_mean_ms={fluidMilliseconds.Average():F3} fluid_step_p95_ms={fluidMilliseconds[(int)(fluidMilliseconds.Count * 0.95)]:F3} changed={changed} max_pending={maxPending} max_reads={maxReads} budget_stops={budgetStops} rendering=none"));
    }
}
