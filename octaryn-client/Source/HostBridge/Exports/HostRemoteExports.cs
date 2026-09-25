using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Client.Host.Remote;

namespace Octaryn.Client.HostBridge;

// Native-callable remote session transport. The graphical client drives these
// through octaryn_client_managed_bridge; the transport mirrors the local
// session mailbox files across the network on its own worker thread.
internal static partial class HostExports
{
    private static RemoteTransportClient? s_remoteTransport;
    private static readonly object s_remoteLock = new();

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_remote_start", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemoteStart(byte* endpointUtf8, byte* runtimeDirectoryUtf8)
    {
        var endpoint = Marshal.PtrToStringUTF8((IntPtr)endpointUtf8);
        var runtimeDirectory = Marshal.PtrToStringUTF8((IntPtr)runtimeDirectoryUtf8);
        if (string.IsNullOrWhiteSpace(endpoint) || string.IsNullOrWhiteSpace(runtimeDirectory))
        {
            return -1;
        }

        lock (s_remoteLock)
        {
            s_remoteTransport?.Dispose();
            s_remoteTransport = new RemoteTransportClient();
            return s_remoteTransport.Start(endpoint, runtimeDirectory, welcomeTimeoutMilliseconds: 8000) ? 0 : -2;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_remote_stop", CallConvs = [typeof(CallConvCdecl)])]
    public static void RemoteStop()
    {
        lock (s_remoteLock)
        {
            s_remoteTransport?.Dispose();
            s_remoteTransport = null;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_remote_is_running", CallConvs = [typeof(CallConvCdecl)])]
    public static int RemoteIsRunning()
    {
        lock (s_remoteLock)
        {
            return s_remoteTransport?.IsRunning == true ? 1 : 0;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "octaryn_client_remote_status", CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemoteStatus(byte* buffer, int capacity)
    {
        if (buffer is null || capacity <= 1)
        {
            return -1;
        }

        string status;
        lock (s_remoteLock)
        {
            status = s_remoteTransport?.Status ?? "stopped";
        }

        var bytes = System.Text.Encoding.UTF8.GetBytes(status);
        var count = Math.Min(bytes.Length, capacity - 1);
        Marshal.Copy(bytes, 0, (IntPtr)buffer, count);
        buffer[count] = 0;
        return count;
    }
}
