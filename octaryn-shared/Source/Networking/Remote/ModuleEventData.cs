using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking.Remote;

// Blittable module replication event carried by the SessionEntity broadcast
// RPC. LES RPC parameters must be non-nullable value types, so module
// payloads are fixed-size: an event id plus three machine words.
[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 32)]
public struct ModuleEventData
{
    public const uint VersionValue = 1;
    public const uint SizeValue = 32;

    public ulong EventId;
    public ulong Payload0;
    public ulong Payload1;
    public ulong Payload2;
}
