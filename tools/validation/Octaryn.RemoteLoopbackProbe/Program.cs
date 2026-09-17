using System.Globalization;
using System.Runtime.InteropServices;
using System.Text.Json;

// Drives a real dedicated server through the native-callable remote session
// exports in octaryn_client_managed_bridge: handshake, authoritative pose and
// chunk snapshot delivery, one consumed block edit, and a reconnect round.
return RemoteLoopbackProbe.Run(args);

internal static class RemoteLoopbackProbe
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int RemoteStartDelegate(IntPtr endpointUtf8, IntPtr runtimeUtf8);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void RemoteStopDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int RemoteIsRunningDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int RemoteStatusDelegate(IntPtr buffer, int capacity);

    public static int Run(string[] args)
    {
        if (!TryParseArgs(args, out var bridge, out var endpoint, out var runtime,
                out var timeoutSeconds, out var error))
        {
            Console.Error.WriteLine($"remote_loopback usage error: {error}");
            return 2;
        }

        Directory.CreateDirectory(runtime);
        WriteIntentFiles(runtime, frameIndex: 1);
        var deadline = DateTime.UtcNow.AddSeconds(timeoutSeconds);

        using var bridgeLibrary = new BridgeLibrary(bridge);
        if (bridgeLibrary.Start(endpoint, runtime) != 0)
        {
            return Fail($"remote start failed: {bridgeLibrary.Status()}");
        }

        Console.WriteLine("remote_loopback connected=1");
        if (!WaitForState(runtime, deadline, bridgeLibrary, out var pose))
        {
            return Fail($"no authoritative state: {bridgeLibrary.Status()}");
        }

        Console.WriteLine(
            $"remote_loopback pose=1 x={pose.X:F3} y={pose.Y:F3} z={pose.Z:F3} tick={pose.Tick}");
        if (!WaitForSnapshot(runtime, deadline))
        {
            return Fail("no chunk snapshot received");
        }

        Console.WriteLine("remote_loopback snapshot=1");
        WriteBlockEdit(runtime, pose);
        if (!WaitForAck(runtime, deadline, bridgeLibrary))
        {
            return Fail($"no block edit acknowledgement: {bridgeLibrary.Status()}");
        }

        Console.WriteLine("remote_loopback block_ack=1");
        bridgeLibrary.Stop();
        Thread.Sleep(TimeSpan.FromMilliseconds(500));
            WriteIntentFiles(runtime, frameIndex: 1);
        if (bridgeLibrary.Start(endpoint, runtime) != 0)
        {
            return Fail($"reconnect failed: {bridgeLibrary.Status()}");
        }

        if (!WaitForState(runtime, DateTime.UtcNow.AddSeconds(60), bridgeLibrary, out _))
        {
            bridgeLibrary.Stop();
            return Fail("no authoritative state after reconnect");
        }

        Console.WriteLine("remote_loopback reconnect=1");
        bridgeLibrary.Stop();
        Console.WriteLine("remote_loopback=passed");
        return 0;
    }

    private static bool TryParseArgs(
        string[] args,
        out string bridge,
        out string endpoint,
        out string runtime,
        out int timeoutSeconds,
        out string? error)
    {
        bridge = string.Empty;
        endpoint = string.Empty;
        runtime = string.Empty;
        timeoutSeconds = 120;
        error = null;
        for (var index = 0; index < args.Length; index++)
        {
            switch (args[index])
            {
                case "--bridge" when TakeValue(args, ref index, out var bridgeValue):
                    bridge = bridgeValue;
                    break;
                case "--endpoint" when TakeValue(args, ref index, out var endpointValue):
                    endpoint = endpointValue;
                    break;
                case "--runtime" when TakeValue(args, ref index, out var runtimeValue):
                    runtime = runtimeValue;
                    break;
                case "--timeout-seconds" when TakeValue(args, ref index, out var timeoutValue):
                    if (!int.TryParse(timeoutValue, out timeoutSeconds) || timeoutSeconds < 10)
                    {
                        error = "--timeout-seconds requires at least 10.";
                        return false;
                    }

                    break;
                default:
                    error = $"unknown argument: {args[index]}";
                    return false;
            }
        }

        if (bridge.Length == 0 || endpoint.Length == 0 || runtime.Length == 0)
        {
            error = "requires --bridge, --endpoint and --runtime.";
            return false;
        }

        return true;
    }

    private static bool TakeValue(string[] args, ref int index, out string value)
    {
        value = string.Empty;
        if (index + 1 >= args.Length)
        {
            return false;
        }

        value = args[++index];
        return true;
    }

    private static int Fail(string message)
    {
        Console.Error.WriteLine($"remote_loopback failed: {message}");
        return 1;
    }

    private static void WriteIntentFiles(string runtime, ulong frameIndex)
    {
        File.WriteAllText(
            Path.Combine(runtime, "chunk_view.json"),
            """{"version":1,"epoch":1,"centerChunkX":0,"centerChunkZ":0,"radius":1,"hasPreviousWindow":false,"previousCenterChunkX":0,"previousCenterChunkZ":0,"previousRadius":0}""");
        File.WriteAllText(
            Path.Combine(runtime, "player_input.json"),
            string.Create(CultureInfo.InvariantCulture,
                $$"""{"version":2,"commands":[{"frameIndex":{{frameIndex}},"flags":0,"controller":1,"moveX":0,"moveY":0,"moveZ":0,"cameraPitch":0,"cameraYaw":0,"relativeMouse":1}]}"""));
    }

    private static void WriteBlockEdit(string runtime, PoseSample pose)
    {
        var editX = (int)Math.Floor(pose.X);
        var editY = (int)Math.Floor(pose.Y) - 2;
        var editZ = (int)Math.Floor(pose.Z);
        File.WriteAllText(
            Path.Combine(runtime, "block_interaction.json"),
            string.Create(CultureInfo.InvariantCulture,
                $$"""{"version":1,"frameIndex":1,"commands":[{"requestId":1,"editX":{{editX}},"editY":{{editY}},"editZ":{{editZ}},"block":0,"cameraX":{{pose.X:R}},"cameraY":{{pose.Y:R}},"cameraZ":{{pose.Z:R}},"hitX":{{editX}},"hitY":{{editY}},"hitZ":{{editZ}}}]}"""));
    }

    private static bool WaitForState(
        string runtime, DateTime deadline, BridgeLibrary bridge, out PoseSample pose)
    {
        pose = default;
        var path = Path.Combine(runtime, "player_state.json");
        while (DateTime.UtcNow < deadline)
        {
            if (TryReadPose(path, out pose))
            {
                return true;
            }

            if (bridge.IsRunning() == 0)
            {
                Console.Error.WriteLine($"remote_loopback transport stopped: {bridge.Status()}");
                return false;
            }

            Thread.Sleep(100);
        }

        return false;
    }

    private static bool WaitForSnapshot(string runtime, DateTime deadline)
    {
        var path = Path.Combine(runtime, "chunk_stream.json.bin");
        while (DateTime.UtcNow < deadline)
        {
            try
            {
                if (new FileInfo(path).Length > 0)
                {
                    return true;
                }
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }

            Thread.Sleep(100);
        }

        return false;
    }

    private static bool WaitForAck(string runtime, DateTime deadline, BridgeLibrary bridge)
    {
        var path = Path.Combine(runtime, "block_interaction.json");
        while (DateTime.UtcNow < deadline)
        {
            if (!File.Exists(path))
            {
                return true;
            }

            if (bridge.IsRunning() == 0)
            {
                Console.Error.WriteLine($"remote_loopback transport stopped: {bridge.Status()}");
                return false;
            }

            Thread.Sleep(100);
        }

        return false;
    }

    private static bool TryReadPose(string path, out PoseSample pose)
    {
        pose = default;
        string payload;
        try
        {
            payload = File.ReadAllText(path);
        }
        catch (IOException)
        {
            return false;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }

        try
        {
            using var document = JsonDocument.Parse(payload);
            var root = document.RootElement;
            if (root.GetProperty("version").GetInt32() != 1 ||
                root.GetProperty("source").GetString() != "server_player_state_stream")
            {
                return false;
            }

            var sample = new PoseSample(
                root.GetProperty("playerX").GetSingle(),
                root.GetProperty("playerY").GetSingle(),
                root.GetProperty("playerZ").GetSingle(),
                root.GetProperty("sourceTick").GetUInt64());
            if (!float.IsFinite(sample.X) || !float.IsFinite(sample.Y) || !float.IsFinite(sample.Z))
            {
                return false;
            }

            pose = sample;
            return true;
        }
        catch (JsonException)
        {
            return false;
        }
        catch (KeyNotFoundException)
        {
            return false;
        }
        catch (InvalidOperationException)
        {
            return false;
        }
    }

    private readonly record struct PoseSample(float X, float Y, float Z, ulong Tick);

    private sealed class BridgeLibrary : IDisposable
    {
        private readonly IntPtr _library;
        private readonly RemoteStartDelegate _start;
        private readonly RemoteStopDelegate _stop;
        private readonly RemoteIsRunningDelegate _isRunning;
        private readonly RemoteStatusDelegate _status;
        private bool _disposed;

        public BridgeLibrary(string path)
        {
            _library = NativeLibrary.Load(path);
            _start = Marshal.GetDelegateForFunctionPointer<RemoteStartDelegate>(
                NativeLibrary.GetExport(_library, "octaryn_client_remote_start"));
            _stop = Marshal.GetDelegateForFunctionPointer<RemoteStopDelegate>(
                NativeLibrary.GetExport(_library, "octaryn_client_remote_stop"));
            _isRunning = Marshal.GetDelegateForFunctionPointer<RemoteIsRunningDelegate>(
                NativeLibrary.GetExport(_library, "octaryn_client_remote_is_running"));
            _status = Marshal.GetDelegateForFunctionPointer<RemoteStatusDelegate>(
                NativeLibrary.GetExport(_library, "octaryn_client_remote_status"));
        }

        public int Start(string endpoint, string runtime)
        {
            var endpointPointer = Marshal.StringToCoTaskMemUTF8(endpoint);
            var runtimePointer = Marshal.StringToCoTaskMemUTF8(runtime);
            try
            {
                return _start(endpointPointer, runtimePointer);
            }
            finally
            {
                Marshal.FreeCoTaskMem(endpointPointer);
                Marshal.FreeCoTaskMem(runtimePointer);
            }
        }

        public void Stop() => _stop();

        public int IsRunning() => _isRunning();

        public string Status()
        {
            var buffer = new byte[256];
            var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            try
            {
                var count = _status(handle.AddrOfPinnedObject(), buffer.Length);
                return count > 0
                    ? System.Text.Encoding.UTF8.GetString(buffer, 0, count)
                    : $"status_error={count}";
            }
            finally
            {
                handle.Free();
            }
        }

        public void Dispose()
        {
            if (_disposed)
            {
                return;
            }

            _disposed = true;
            NativeLibrary.Free(_library);
        }
    }
}
