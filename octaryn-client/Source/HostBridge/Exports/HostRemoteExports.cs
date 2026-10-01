using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Client.Host.Remote;

namespace Octaryn.Client.HostBridge;

// Native-callable remote session transport. The graphical client drives these
// through octaryn_client_managed_bridge; the transport owns bounded in-memory
// channels and network processing on its own worker thread.
internal static partial class HostExports
{
    private static RemoteTransportClient? s_remoteTransport;
    private static readonly object s_remoteLock = new();

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_remote_start", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemoteStart(byte* endpointUtf8, byte* runtimeDirectoryUtf8) =>
        StartRemote(endpointUtf8, runtimeDirectoryUtf8, 8000);

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemoteStartAsync(byte* endpointUtf8, byte* runtimeDirectoryUtf8) =>
        StartRemote(endpointUtf8, runtimeDirectoryUtf8, 0);

    private static unsafe int StartRemote(byte* endpointUtf8, byte* runtimeDirectoryUtf8, int timeout)
    {
        var endpoint = Marshal.PtrToStringUTF8((IntPtr)endpointUtf8);
        var runtimeDirectory = Marshal.PtrToStringUTF8((IntPtr)runtimeDirectoryUtf8);
        if (string.IsNullOrWhiteSpace(endpoint) || string.IsNullOrWhiteSpace(runtimeDirectory))
        {
            return -1;
        }

        lock (s_remoteLock)
        {
            Interlocked.Exchange(ref s_remoteTransport, null)?.Dispose();
            var transport = new RemoteTransportClient();
            var started = transport.Start(endpoint, runtimeDirectory, welcomeTimeoutMilliseconds: timeout);
            // Publish only after Start has initialized and cleared its channels.
            // The native journal worker may submit concurrently with startup.
            Volatile.Write(ref s_remoteTransport, transport);
            return started ? 0 : -2;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_remote_stop", CallConvs = [typeof(CallConvCdecl)])]
    public static void RemoteStop()
    {
        lock (s_remoteLock)
        {
            Interlocked.Exchange(ref s_remoteTransport, null)?.Dispose();
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_remote_is_running", CallConvs = [typeof(CallConvCdecl)])]
    public static int RemoteIsRunning()
    {
        return Volatile.Read(ref s_remoteTransport)?.IsRunning == true ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_remote_status", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemoteStatus(byte* buffer, int capacity)
    {
        if (buffer is null || capacity <= 1)
        {
            return -1;
        }

        var status = Volatile.Read(ref s_remoteTransport)?.Status ?? "stopped";

        var count = System.Text.Encoding.UTF8.GetByteCount(status);
        if (count < capacity)
            count = System.Text.Encoding.UTF8.GetBytes(status.AsSpan(), new Span<byte>(buffer, capacity - 1));
        else
        {
            var bytes = System.Text.Encoding.UTF8.GetBytes(status);
            count = capacity - 1;
            Marshal.Copy(bytes, 0, (IntPtr)buffer, count);
        }
        buffer[count] = 0;
        return count;
    }

    // Pops one queued module event (grant, drop receipt, look target) for the
    // native frame loop. Returns 1 when an event was written, 0 when empty.
    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_remote_poll_module_event", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemotePollModuleEvent(ulong* eventId, ulong* kind, ulong* payload1, ulong* payload2)
    {
        if (eventId is null || kind is null || payload1 is null || payload2 is null)
        {
            return -1;
        }

        var transport = Volatile.Read(ref s_remoteTransport);

        if (transport is null || !transport.TryPollModuleEvent(out var data))
        {
            return 0;
        }

        *eventId = data.EventId;
        *kind = data.Payload0;
        *payload1 = data.Payload1;
        *payload2 = data.Payload2;
        return 1;
    }
}
