using System.Runtime.InteropServices;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;

namespace Octaryn.Server.HostBridge;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 48)]
internal unsafe struct NativeHostApi
{
    public const uint VersionValue = 1;
    public const uint SizeValue = 48;

    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<HostCommand*, int> EnqueueHostCommand;
    public delegate* unmanaged[Cdecl]<ServerSnapshotHeader*, int> PublishServerSnapshot;
    public delegate* unmanaged[Cdecl]<ClientCommandFrame*, int> PollClientCommands;
    public ulong Reserved;
    public ulong Reserved1;
}
