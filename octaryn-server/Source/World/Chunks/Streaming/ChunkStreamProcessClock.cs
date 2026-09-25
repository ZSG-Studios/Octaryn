using Octaryn.Server.Modules;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static unsafe partial class ChunkStreamProcessBridge
{
    private static ulong s_acknowledgedInputFrame;
    private static ulong s_worldTick;
    private static readonly FixedStepBudget s_worldBudget = new();
    internal static bool CommandAuthorityActive { get; private set; }
    internal static ulong ConsumedPlayerCommand => s_playerCommands.Acknowledged;
    internal static void AcceptRemotePlayerCommands(ReadOnlySpan<Octaryn.Shared.Networking.Remote.PlayerCommand> commands)
        => s_playerCommands.Accept(commands);
    internal static (ulong Tick, double Seconds) PlayerSourceClock => (s_sourceTick, s_sourceSeconds);

    internal enum CommandDependency { Ready, Wait, Reject }

    internal static CommandDependency EvaluateCommandDependency(ulong inputFrame, double waitingSeconds)
    {
        if (inputFrame <= ConsumedPlayerCommand) return CommandDependency.Ready;
        return inputFrame - ConsumedPlayerCommand > 256 || waitingSeconds >= 0.5
            ? CommandDependency.Reject : CommandDependency.Wait;
    }

    internal static void ResetSessionState()
    {
        s_sourceTick = 0;
        s_acknowledgedInputFrame = 0;
        s_sourceSeconds = 0;
        s_snapshotContentionCount = 0;
        s_playerPublication.Reset();
        s_playerCommands.Reset();
        s_blockAdmission.Reset();
        s_worldTick = 0;
        s_worldBudget.Reset();
        CommandAuthorityActive = false;
    }

    internal static int ExecuteTrackedPlayerTick(ModuleActivator gameModule,
        in HostFrameSnapshot frame, NativeChunkStreamProcessTickDecision decision)
    {
        CommandAuthorityActive = true;
        s_worldBudget.Accrue();
        for (var step = 0; step < FixedStepBudget.MaximumSteps && s_worldBudget.CanStep; step++)
        {
            var worldFrame = new HostFrameSnapshot(frame.Input, new HostFrameTimingSnapshot(
                HostFrameTimingSnapshot.VersionValue, HostFrameTimingSnapshot.SizeValue,
                s_worldTick + 1, PlayerCommandQueue.FixedDelta));
            var fixedDecision = new NativeChunkStreamProcessTickDecision(1, decision.UseHostOnlyTick, 0);
            var result = ChunkStreamProcessTickBridge.Execute(gameModule, in worldFrame, fixedDecision);
            if (result != 0) return result;
            s_worldTick++;
            s_worldBudget.Commit();
        }
        return 0;
    }

    internal static void ConsumePlayerCommands(PlayerState state, Action<HostFrameContext> consume)
    {
        s_playerCommands.SeedIdleView(state.Pitch, state.Yaw, state.ControlMode);
        for (var step = 0; step < FixedStepBudget.MaximumSteps &&
            s_playerCommands.TrySelect(s_sourceTick + 1, out var frame); step++)
        {
            consume(HostFrameContext.FromSnapshot(in frame));
            s_playerCommands.Commit(in frame);
            s_sourceTick++;
            s_sourceSeconds = s_sourceTick * PlayerCommandQueue.FixedDelta;
        }
    }
}
