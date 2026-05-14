using System.Runtime.InteropServices;

namespace Octaryn.Engine.Api;

[StructLayout(LayoutKind.Sequential, Size = 152)]
public readonly struct OctarynFrameSnapshot
{
    public const uint VersionValue = 1;

    public readonly uint Version;
    public readonly uint Size;
    public readonly OctarynInputSnapshot Input;
    public readonly OctarynWorldSnapshot World;
}
