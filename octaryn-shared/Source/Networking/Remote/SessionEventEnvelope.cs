using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking.Remote;

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct SessionEventEnvelope
{
    public ulong Sequence;
    public ModuleEventData Data;
}
