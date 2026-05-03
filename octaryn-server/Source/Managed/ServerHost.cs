using Octaryn.Shared.Host;

namespace Octaryn.Server;

public static class ServerHost
{
    private const string ReadySignal = "octaryn_server_ready=1";
    private const string ShutdownSignal = "octaryn_server_shutdown=1";

    public static int Run(IReadOnlyList<string> args)
    {
        ServerLiveDebugLog.Write($"server_live_startup args={args.Count}");
        var basegame = new ServerModuleActivator();
        try
        {
            var activateResult = basegame.Activate(new ServerConsoleCommandSink());
            ServerLiveDebugLog.Write($"server_live_startup_activate result={activateResult}");
            if (activateResult != 0)
            {
                return activateResult;
            }

            basegame.Tick(CreateStartupFrame());
            ServerLiveDebugLog.Write($"server_live_readiness ready=1 world_blocks={basegame.WorldBlockCount} pending_block_changes={basegame.PendingBlockChangeCount}");
            Console.WriteLine(ReadySignal);
        }
        finally
        {
            basegame.Dispose();
        }

        Console.WriteLine(ShutdownSignal);
        return 0;
    }

    private static HostFrameSnapshot CreateStartupFrame()
    {
        return new HostFrameSnapshot(
            new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue),
            new HostFrameTimingSnapshot(
                HostFrameTimingSnapshot.VersionValue,
                HostFrameTimingSnapshot.SizeValue,
                frameIndex: 0,
                deltaSeconds: 1.0 / 60.0));
    }
}
