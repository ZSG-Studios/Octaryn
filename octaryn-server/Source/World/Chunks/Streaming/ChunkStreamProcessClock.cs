using Octaryn.Server.Modules;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.Host;

namespace Octaryn.Server;

internal static unsafe partial class ChunkStreamProcessBridge
{
    private static ulong s_acknowledgedInputFrame;
    internal static (ulong Tick, double Seconds) PlayerSourceClock => (s_sourceTick, s_sourceSeconds);

 // Dedicated servers host sequential sessions in one process; each attach starts
 // from a clean publication watermark so the first pose reaches the new peer.
 internal static void ResetSessionState()
 {
        s_sourceTick = 0;
        s_acknowledgedInputFrame = 0;
 s_sourceSeconds = 0;
 s_snapshotContentionCount = 0;
 s_playerPublication.Reset();
 }

    internal static int ExecuteTrackedPlayerTick(ModuleActivator gameModule,
        in HostFrameSnapshot frame, NativeChunkStreamProcessTickDecision decision)
    {
        var result = ChunkStreamProcessTickBridge.Execute(gameModule, in frame, decision);
        if (result == 0 && decision.ShouldTick != 0)
        {
            // Host-only execution still advances the authoritative player and world.
            s_sourceTick++;
            s_sourceSeconds += decision.UseDefaultFrame != 0
                ? 1.0 / 60.0 : Math.Clamp(frame.Timing.DeltaSeconds, 0.0, 0.25);
        }
        return result;
    }
}
