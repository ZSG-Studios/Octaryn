using System.Diagnostics;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static class PlayerInputFreshness
{
    private static ulong s_lastFrame;
    private static long s_lastReceive;
    private static HostInputSnapshot s_latest;

    public static HostFrameSnapshot Apply(HostFrameSnapshot frame)
    {
        var now = Stopwatch.GetTimestamp();
        if (s_lastReceive == 0 || frame.Timing.FrameIndex > s_lastFrame)
        {
            s_lastFrame = frame.Timing.FrameIndex;
            s_lastReceive = now;
            s_latest = frame.Input;
        }

        var input = s_latest;
        if (Stopwatch.GetElapsedTime(s_lastReceive, now).TotalSeconds > 0.25)
        {
            input = new HostInputSnapshot(input.Version, input.Size,
                input.Flags & HostInputSnapshot.FlyModeFlag, 1,
                0, 0, 0, input.CameraX, input.CameraY, input.CameraZ,
                input.CameraPitch, input.CameraYaw, 1);
        }
        return new HostFrameSnapshot(input, new HostFrameTimingSnapshot(
            frame.Timing.Version, frame.Timing.Size, s_lastFrame, frame.Timing.DeltaSeconds));
    }
}
