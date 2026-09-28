using System.Runtime.InteropServices;

namespace Octaryn.Shared.Host.Api;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 64)]
public struct HostWorldItemPose
{
    public const uint Removed = 1, Sleeping = 2;
    public ulong EntityId, Generation, SourceTick;
    public float X, Y, Z, VelocityX, VelocityY, VelocityZ;
    public uint ItemId, Count, Flags, Reserved;

    public readonly bool IsValid => EntityId != 0 && (Flags & ~3u) == 0 && Reserved == 0 &&
        ((Flags & Removed) != 0 || (ItemId != 0 && Count != 0 &&
        float.IsFinite(X) && float.IsFinite(Y) && float.IsFinite(Z) &&
        float.IsFinite(VelocityX) && float.IsFinite(VelocityY) && float.IsFinite(VelocityZ)));
}
