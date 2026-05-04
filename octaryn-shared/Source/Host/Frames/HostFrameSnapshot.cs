using System.Runtime.InteropServices;

namespace Octaryn.Shared.Host;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 88)]
internal readonly struct HostFrameSnapshot
{
    public const uint VersionValue = 1;
    public const uint SizeValue = 88;

    public readonly uint Version;
    public readonly uint Size;
    public readonly HostInputSnapshot Input;
    public readonly HostFrameTimingSnapshot Timing;

    internal HostFrameSnapshot(HostInputSnapshot input, HostFrameTimingSnapshot timing)
    {
        Version = VersionValue;
        Size = SizeValue;
        Input = input;
        Timing = timing;
    }
}
