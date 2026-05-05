using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.Time;
using Octaryn.Shared.Host;
using Octaryn.Shared.Time;

namespace Octaryn.Server.Tick;

internal sealed class AuthorityTickRunner(
    NativeScheduleRuntime scheduleRuntime,
    PlayerController playerController,
    WorldTimeClock worldTime)
{
    public WorldTime Execute(in HostFrameContext frame)
    {
        var tickFrame = frame;
        WorldTime tickTime = default;
        scheduleRuntime.ExecuteWorker(
            "server.authority.tick",
            () =>
            {
                playerController.Tick(in tickFrame);
                tickTime = worldTime.AdvanceFrame(tickFrame.DeltaSeconds);
            });
        return tickTime;
    }
}
