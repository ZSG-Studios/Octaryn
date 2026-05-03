using Octaryn.Shared.Host;

namespace Octaryn.Server;

public static class ServerHost
{
    private const string ReadySignal = "octaryn_server_ready=1";
    private const string ShutdownSignal = "octaryn_server_shutdown=1";

    public static int Run(IReadOnlyList<string> args)
    {
        _ = args;
        var basegame = new ServerModuleActivator();
        try
        {
            var activateResult = basegame.Activate(new ServerConsoleCommandSink());
            if (activateResult != 0)
            {
                return activateResult;
            }

            basegame.Tick(CreateStartupFrame());
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
