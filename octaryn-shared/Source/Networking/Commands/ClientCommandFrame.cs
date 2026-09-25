using System.Runtime.InteropServices;

namespace Octaryn.Shared.Networking;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 32)]
public readonly struct ClientCommandFrame
{
    public const uint VersionValue = 1u;
    public const uint SizeValue = 32u;

    public readonly uint Version;
    public readonly uint Size;
    public readonly uint CommandCount;
    public readonly uint Reserved;
    public readonly ulong TickId;
    public readonly ulong CommandsAddress;

    internal ClientCommandFrame(
        uint commandCount,
        ulong tickId,
        ulong commandsAddress)
    {
        Version = VersionValue;
        Size = SizeValue;
        CommandCount = commandCount;
        Reserved = 0;
        TickId = tickId;
        CommandsAddress = commandsAddress;
    }
}
