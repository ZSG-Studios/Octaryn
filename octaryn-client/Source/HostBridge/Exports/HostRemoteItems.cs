using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Client.Host.Remote;

namespace Octaryn.Client.HostBridge;

internal static partial class HostExports
{
    // -2 means unchanged, -1 invalid arguments, otherwise a coherent array count.
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemoteCopyWorldItems(ulong* revision, WorldItemVisualPose* output, int capacity, int stride)
    {
        if (revision is null || output is null || capacity < 10000 || stride != sizeof(WorldItemVisualPose)) return -1;
        var snapshot = Volatile.Read(ref s_remoteTransport)?.WorldItems;
        if (snapshot is null || snapshot.Revision == *revision) return -2;
        snapshot.Poses.AsSpan().CopyTo(new Span<WorldItemVisualPose>(output, capacity));
        *revision = snapshot.Revision;
        return snapshot.Poses.Length;
    }
}
