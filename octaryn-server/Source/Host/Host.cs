using Octaryn.Server.Modules;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Host;

public static class Host
{
    private static readonly TimeSpan LiveChunkStreamInterval = TimeSpan.FromMilliseconds(50);

    private const string ReadySignal = "octaryn_server_ready=1";
    private const string ShutdownSignal = "octaryn_server_shutdown=1";
    private const string LiveStreamEnvironmentVariable = "OCTARYN_SERVER_PROCESS_STREAM_LIVE";
    private const string DisableGameModulesEnvironmentVariable = "OCTARYN_SERVER_DISABLE_GAME_MODULES";
    private const string ClientDisableGameModulesEnvironmentVariable = "OCTARYN_CLIENT_DISABLE_GAME_MODULES";

    public static int Run(IReadOnlyList<string> args)
    {
        LiveDebugLog.Write($"server_live_startup args={args.Count}");
        var gameModule = ShouldDisableGameModules()
            ? ModuleActivator.CreateWithoutGameModules()
            : new ModuleActivator();
        try
        {
            var activateResult = gameModule.Activate(new ConsoleCommandSink());
            LiveDebugLog.Write($"server_live_startup_activate result={activateResult}");
            if (activateResult != 0)
            {
                return activateResult;
            }

            gameModule.Tick(CreateStartupFrame());
            LiveDebugLog.Write($"server_live_readiness ready=1 world_blocks={gameModule.WorldBlockCount} pending_block_changes={gameModule.PendingBlockChangeCount}");
            if (IsEnabled(LiveStreamEnvironmentVariable))
            {
                Console.WriteLine(ReadySignal);
                return RunLiveChunkStream(gameModule);
            }

            var chunkStreamResult = ChunkStreamProcessBridge.HandleIfRequested(gameModule);
            if (chunkStreamResult != 0)
            {
                return chunkStreamResult;
            }

            Console.WriteLine(ReadySignal);
        }
        finally
        {
            gameModule.Dispose();
        }

        Console.WriteLine(ShutdownSignal);
        return 0;
    }

    private static int RunLiveChunkStream(ModuleActivator gameModule)
    {
        LiveDebugLog.Write("server_live_process_stream active=1 mode=background");
        while (true)
        {
            var chunkStreamResult = ChunkStreamProcessBridge.HandleIfRequested(gameModule, allowMissingIntent: true);
            if (chunkStreamResult != 0)
            {
                return chunkStreamResult;
            }

            Thread.Sleep(LiveChunkStreamInterval);
        }
    }

    private static bool ShouldDisableGameModules()
    {
        return IsEnabled(DisableGameModulesEnvironmentVariable) ||
            IsEnabled(ClientDisableGameModulesEnvironmentVariable);
    }

    private static bool IsEnabled(string name)
    {
        var value = Environment.GetEnvironmentVariable(name);
        return !string.IsNullOrWhiteSpace(value) &&
            (value == "1" ||
             value.Equals("true", StringComparison.OrdinalIgnoreCase) ||
             value.Equals("yes", StringComparison.OrdinalIgnoreCase));
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
