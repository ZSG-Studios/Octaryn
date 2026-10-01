using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking.Remote;

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct SessionWelcome
{
    public ulong Version, SessionHigh, SessionLow, AcknowledgedInput, Resumed;
}
