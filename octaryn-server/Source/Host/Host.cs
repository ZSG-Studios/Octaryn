using Octaryn.Server.Modules;
using Octaryn.Server.World.Chunks;

namespace Octaryn.Server.Host;

public static class Host
{
    private const string ReadySignal = "octaryn_server_ready=1";
    private const string ShutdownSignal = "octaryn_server_shutdown=1";

    public static int Run(IReadOnlyList<string> args)
    {
        var startupPolicy = NativeHostPolicyLibrary.GetStartupPolicy();
        LiveDebugLog.Write($"server_live_startup args={args.Count}");
        var gameModule = startupPolicy.DisableGameModules
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

            gameModule.Tick(NativeHostPolicyLibrary.CreateStartupFrame());
            LiveDebugLog.Write($"server_live_readiness ready=1 world_blocks={gameModule.WorldBlockCount} pending_block_changes={gameModule.PendingBlockChangeCount}");
            if (startupPolicy.LiveProcessStream)
            {
                Console.WriteLine(ReadySignal);
                return RunLiveChunkStream(gameModule, startupPolicy.LiveStreamIntervalMilliseconds);
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

    private static int RunLiveChunkStream(ModuleActivator gameModule, uint intervalMilliseconds)
    {
        LiveDebugLog.Write("server_live_process_stream active=1 mode=background");
        return NativeHostPolicyLibrary.RunLiveStreamLoop(
            intervalMilliseconds,
            () => ChunkStreamProcessBridge.HandleIfRequested(gameModule, allowMissingIntent: true));
    }
}
