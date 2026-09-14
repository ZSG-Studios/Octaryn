using Octaryn.Server.Modules;
using Octaryn.Server.World.Chunks;
using Octaryn.Server.World.Items;

namespace Octaryn.Server.Host;

public static class Host
{
    private const string ReadySignal = "octaryn_server_ready=1";
    private const string ShutdownSignal = "octaryn_server_shutdown=1";

    public static int Run(IReadOnlyList<string> args)
    {
        var startupPolicy = NativeHostPolicyLibrary.GetStartupPolicy();
        LiveDebugLog.Write($"server_live_startup args={args.Count}");
        var gameModule = new ModuleActivator(BlockPublicationMode.ProcessSnapshots);
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
        using var items = new WorldItemsProcess(gameModule);
        LiveDebugLog.Write("server_live_process_stream active=1 mode=background");
        var shutdownPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH");
        var result = NativeHostPolicyLibrary.RunLiveStreamLoop(
            intervalMilliseconds,
            () => !string.IsNullOrWhiteSpace(shutdownPath) && File.Exists(shutdownPath)
                ? 1
                : RunLiveStep(gameModule, items));
        if (result == 1 && !string.IsNullOrWhiteSpace(shutdownPath) && File.Exists(shutdownPath))
        {
            Console.WriteLine(ShutdownSignal);
            return 0;
        }
        return result;
    }

    private static int RunLiveStep(ModuleActivator gameModule, WorldItemsProcess items)
    {
        var result = ChunkStreamProcessBridge.HandleIfRequested(gameModule, allowMissingIntent: true);
        if (result != 0) return result;
        try { items.Step(); }
        catch (Exception ex) when (WorldItemsProcess.IsTransientFileContention(ex))
        {
            LiveDebugLog.Write($"server_world_items deferred=1 reason=file_contention error={ex.GetType().Name} code={ex.HResult & 0xffff}");
        }
        return 0;
    }
}
