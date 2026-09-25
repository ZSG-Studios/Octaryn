using System.Runtime.InteropServices;

namespace Octaryn.Shared.Host;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
internal readonly struct HostFrameTimingSnapshot
{
    public const uint VersionValue = 1;
    public const uint SizeValue = 24;

    public readonly uint Version;
    public readonly uint Size;
    public readonly ulong FrameIndex;
    public readonly double DeltaSeconds;

    internal HostFrameTimingSnapshot(
        uint version,
        uint size,
        ulong frameIndex,
        double deltaSeconds)
    {
        Version = version;
        Size = size;
        FrameIndex = frameIndex;
        DeltaSeconds = deltaSeconds;
    }
}
