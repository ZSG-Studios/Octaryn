using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.Host;

namespace Octaryn.Basegame.Module;

public static class ScheduleDeclarations
{
    public static ScheduledSystemDeclaration FrameTick { get; } = new(
        SystemId: "octaryn.basegame.frame_tick",
        Phase: HostWorkPhase.Gameplay,
        FrameOrTickOwner: HostScheduleIds.FrameOrTickOwner,
        Reads:
        [
            new ScheduledResourceAccess(HostApiIds.Frame, ScheduledAccessMode.Read)
        ],
        Writes:
        [
            new ScheduledResourceAccess("octaryn.basegame.frame_state", ScheduledAccessMode.Write),
            new ScheduledResourceAccess(HostApiIds.Commands, ScheduledAccessMode.Write)
        ],
        RunsAfter: [],
        RunsBefore: [],
        Flags: HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier,
        CommitBarrier: HostScheduleIds.FrameOrTickEndBarrier);
}
