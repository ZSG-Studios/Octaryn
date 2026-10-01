using System.Reflection;
using System.Runtime.Loader;
using System.Runtime.InteropServices;

internal static class ClientStatusProbe
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ReadRunning();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] private delegate int ReadStatus(IntPtr text, int capacity);
    public static void Run(string serverBundle)
    {
        var bundle = Path.GetFullPath(Path.Combine(serverBundle, "../../client/bundle"));
        var assembly = AssemblyLoadContext.Default.LoadFromAssemblyPath(Path.Combine(bundle, "Octaryn.Client.dll"));
        var type = assembly.GetType("Octaryn.Client.Host.Remote.RemoteTransportClient", true)!;
        const BindingFlags members = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance;
        using var transport = (IDisposable)Activator.CreateInstance(type, true)!;
        var mutex = type.GetField("_mutex", members)!.GetValue(transport)!;
        var status = type.GetProperty("Status", members)!;
        var running = type.GetProperty("IsRunning", members)!;
        using var readStarted = new ManualResetEventSlim();
        Task<(string, bool)> read;
        lock (mutex)
        {
            // Simulate the worker retaining the shared lock. Frame status reads
            // must complete before we release it, not merely return eventually.
            read = Task.Run(() =>
            {
                readStarted.Set();
                return ((string)status.GetValue(transport)!, (bool)running.GetValue(transport)!);
            });
            if (!readStarted.Wait(TimeSpan.FromSeconds(2)) || !read.Wait(TimeSpan.FromSeconds(2)))
                throw new InvalidOperationException("Frame status read waited for transport mutex");
        }
        if (read.Result != ("stopped", false))
            throw new InvalidOperationException("Incorrect initial immutable transport status");
        var exports = assembly.GetType("Octaryn.Client.HostBridge.HostExports", true)!;
        const BindingFlags staticMembers = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static;
        var exportMutex = exports.GetField("s_remoteLock", staticMembers)!.GetValue(null)!;
        var remoteField = exports.GetField("s_remoteTransport", staticMembers)!;
        remoteField.SetValue(null, transport);
        try
        {
            var nativeRunning = Marshal.GetDelegateForFunctionPointer<ReadRunning>(
                exports.GetMethod("RemoteIsRunning", staticMembers)!.MethodHandle.GetFunctionPointer());
            var nativeStatus = Marshal.GetDelegateForFunctionPointer<ReadStatus>(
                exports.GetMethod("RemoteStatus", staticMembers)!.MethodHandle.GetFunctionPointer());
            lock (exportMutex)
            {
                var nativeRead = Task.Run(() => {
                    var buffer = Marshal.AllocHGlobal(64);
                    try { return nativeRunning() == 0 && nativeStatus(buffer, 64) == 7 && Marshal.PtrToStringUTF8(buffer) == "stopped"; }
                    finally { Marshal.FreeHGlobal(buffer); }
                });
                if (!nativeRead.Wait(TimeSpan.FromSeconds(2)) || !nativeRead.Result)
                    throw new InvalidOperationException("Native frame status ABI waited behind Start/Stop lifecycle lock");
            }
        }
        finally { remoteField.SetValue(null, null); }
        var submitIntent = type.GetMethod("SubmitIntent", members)!;
        if ((bool)submitIntent.Invoke(transport, [(byte)5, new byte[65537]])! ||
            (bool)submitIntent.Invoke(transport, [(byte)99, new byte[1]])! ||
            (bool)submitIntent.Invoke(transport, [(byte)5, "journal"u8.ToArray()])!)
            throw new InvalidOperationException("Intent channel did not enforce kind and capacity");
        var poseType = type.GetMethod("PublishTypedPose", members)!.GetParameters()[0].ParameterType;
        var pose = Activator.CreateInstance(poseType)!;
        poseType.GetField("SourceTick")!.SetValue(pose, 41UL);
        poseType.GetField("X")!.SetValue(pose, 3.5f);
        type.GetMethod("PublishTypedPose", members)!.Invoke(transport, [pose]);
        var result = new object?[] { null };
        var poll = type.GetMethod("TryPollPose", members)!;
        if (!(bool)poll.Invoke(transport, result)! ||
            (ulong)poseType.GetField("SourceTick")!.GetValue(result[0])! != 41 ||
            (float)poseType.GetField("X")!.GetValue(result[0])! != 3.5f ||
            (bool)poll.Invoke(transport, result)!)
            throw new InvalidOperationException("Typed pose exchange lost data or replayed consumed state");
        Console.WriteLine("client_status_probe=passed lock_independent=1");
    }
}
