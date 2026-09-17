using Octaryn.Server.Modules;
using Octaryn.Server.Networking.Remote;
using Octaryn.Server.World.Chunks;
using Octaryn.Server.World.Items;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Server.Host;

public static class Host
{
    private const string ReadySignal = "octaryn_server_ready=1";
    private const string ShutdownSignal = "octaryn_server_shutdown=1";

    public static int Run(IReadOnlyList<string> args)
    {
        if (!TryParseRemoteOptions(args, out var remote, out var oneShot, out var showHelp, out var remoteError))
        {
            Console.Error.WriteLine(remoteError);
            Console.Error.WriteLine(RemoteUsage);
            return 2;
        }

        if (showHelp)
        {
            Console.WriteLine(RemoteUsage);
            return 0;
        }

        if (oneShot)
        {
            Environment.SetEnvironmentVariable("OCTARYN_SERVER_PROCESS_STREAM_LIVE", "0");
        }

        if (remote is not null)
        {
            ApplyWorldDirectory(remote.WorldDirectory);
            Environment.SetEnvironmentVariable("OCTARYN_SERVER_PROCESS_STREAM_LIVE", "1");
            SetEnvironmentDefault("OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY", "1");
            // Keep a durable activity log beside the world even when the console
            // is not attached or its output is missed.
            if (string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH")))
            {
                Environment.SetEnvironmentVariable(
                    "OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH",
                    Path.Combine(remote.WorldDirectory, "logs", "server.log"));
            }
        }

        var startupPolicy = NativeHostPolicyLibrary.GetStartupPolicy();
        LiveDebugLog.Write($"server_live_startup args={args.Count}");
        var gameModule = new ModuleActivator(BlockPublicationMode.ProcessSnapshots);
        var exitCode = 0;
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
            if (remote is not null)
            {
                return RunRemoteServer(gameModule, remote);
            }

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
        return exitCode;
    }

    private static int RunRemoteServer(ModuleActivator gameModule, RemoteOptions remote)
    {
        var shutdownPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH");
        using var server = new RemoteServer(
            gameModule,
            Path.Combine(remote.WorldDirectory, "remote-session"),
            remote.IntervalMilliseconds,
            () => !string.IsNullOrWhiteSpace(shutdownPath) && File.Exists(shutdownPath));
        LiveDebugLog.Write($"server_remote_start active=1 world={remote.WorldDirectory} endpoint={remote.Host}:{remote.Port}");
        return server.Execute(remote.Host, remote.Port);
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

    private const string RemoteUsage =
        "Usage: Octaryn.Server [--listen [host:]port] [--world-dir path] [--one-shot] [--help]\n" +
        "  Starts a dedicated server by default (port 17531; binds all interfaces).\n" +
        "  --listen  Override the dedicated server endpoint.\n" +
        "  --world-dir  World save directory (default ./octaryn-world).\n" +
        "  --one-shot  Run one readiness/intent pass and exit (validation only; excludes --listen).\n" +
        "  Supervised clients select process streaming with OCTARYN_SERVER_PROCESS_STREAM_LIVE.";

    private sealed record RemoteOptions(string Host, int Port, string WorldDirectory, uint IntervalMilliseconds);

    private static bool TryParseRemoteOptions(
        IReadOnlyList<string> args, out RemoteOptions? remote, out bool oneShot, out bool showHelp, out string? error)
    {
        remote = null;
        oneShot = false;
        showHelp = false;
        error = null;
        string? listen = null;
        var listenRequested = false;
        string? worldDirectory = null;

        for (var index = 0; index < args.Count; index++)
        {
            var arg = args[index];
            if (arg is "--help" or "-h")
            {
                showHelp = true;
                return true;
            }

            if (arg == "--one-shot")
            {
                oneShot = true;
                continue;
            }

            if (arg == "--listen" || arg.StartsWith("--listen=", StringComparison.Ordinal))
            {
                listenRequested = true;
                listen = arg == "--listen"
                    ? (index + 1 < args.Count && !args[index + 1].StartsWith("--", StringComparison.Ordinal) ? args[++index] : null)
                    : arg["--listen=".Length..];
                continue;
            }

            if (arg == "--world-dir" || arg.StartsWith("--world-dir=", StringComparison.Ordinal))
            {
                worldDirectory = arg == "--world-dir"
                    ? (index + 1 < args.Count ? args[++index] : null)
                    : arg["--world-dir=".Length..];
                if (string.IsNullOrWhiteSpace(worldDirectory))
                {
                    error = "--world-dir requires a path.";
                    return false;
                }

                continue;
            }

            if (arg.StartsWith("--", StringComparison.Ordinal))
            {
                error = $"Unknown server option: {arg}";
                return false;
            }
        }

        if (oneShot)
        {
            if (listenRequested)
            {
                error = "--one-shot cannot be combined with --listen.";
                return false;
            }

            return true;
        }

        worldDirectory ??= Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_DIR");
        worldDirectory = string.IsNullOrWhiteSpace(worldDirectory)
            ? Path.Combine(Directory.GetCurrentDirectory(), "octaryn-world")
            : Path.GetFullPath(worldDirectory);

        if (!listenRequested)
        {
            listen = Environment.GetEnvironmentVariable("OCTARYN_SERVER_LISTEN");
            listenRequested = !string.IsNullOrWhiteSpace(listen);
        }

        if (!listenRequested && NativeHostPolicyLibrary.GetStartupPolicy().LiveProcessStream)
        {
            return true;
        }

        if (!TryParseListenEndpoint(listen, out var host, out var port, out error))
        {
            return false;
        }

        var interval = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PROCESS_STREAM_INTERVAL_MS");
        remote = new RemoteOptions(
            host,
            port,
            worldDirectory,
            uint.TryParse(interval, out var parsed) && parsed != 0 ? parsed : 16u);
        return true;
    }

    private static bool TryParseListenEndpoint(
        string? value, out string host, out int port, out string? error)
    {
        host = string.Empty;
        port = RemoteProtocol.DefaultPort;
        error = null;
        if (string.IsNullOrWhiteSpace(value))
        {
            return true;
        }

        var text = value.Trim();
        if (text.StartsWith('['))
        {
            var bracket = text.IndexOf(']');
            if (bracket < 0 || bracket + 1 >= text.Length || text[bracket + 1] != ':')
            {
                error = $"Invalid --listen endpoint: {value}. Use [host:]port.";
                return false;
            }

            host = text[1..bracket];
            text = text[(bracket + 2)..];
        }
        else
        {
            var separator = text.LastIndexOf(':');
            if (separator > 0)
            {
                host = text[..separator];
                text = text[(separator + 1)..];
            }
            else if (separator == 0)
            {
                error = $"Invalid --listen endpoint: {value}. Use [host:]port.";
                return false;
            }
        }

        if (!int.TryParse(text, out port) || port < 1 || port > 65535)
        {
            error = $"Invalid --listen port: {value}. Use [host:]port with port 1-65535.";
            return false;
        }

        return true;
    }

    private static void ApplyWorldDirectory(string worldDirectory)
    {
        Directory.CreateDirectory(worldDirectory);
        SetEnvironmentDefault(
            "OCTARYN_SERVER_WORLD_BLOCKS_PATH",
            Path.Combine(worldDirectory, "world_blocks.json"));
        SetEnvironmentDefault("OCTARYN_SERVER_PLAYER_SAVE_ROOT", worldDirectory);
    }

    private static void SetEnvironmentDefault(string name, string value)
    {
        if (string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable(name)))
        {
            Environment.SetEnvironmentVariable(name, value);
        }
    }
}
