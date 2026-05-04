using System.Runtime.InteropServices;
using Octaryn.Shared.Host;

namespace Octaryn.Client.HostBridge;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 16)]
internal unsafe struct NativeHostApi
{
    public const uint VersionValue = 1;
    public const uint SizeValue = 16;

    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<HostCommand*, int> EnqueueCommand;
}
