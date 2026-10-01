using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.World;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistencePlayerState(
    float x,
    float y,
    float z,
    float pitch,
    float yaw)
{
    public readonly float X = x;
    public readonly float Y = y;
    public readonly float Z = z;
    public readonly float Pitch = pitch;
    public readonly float Yaw = yaw;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativePersistencePlayerFileEntry(int playerId, NativePersistencePlayerState state)
{
    public readonly int PlayerId = playerId;
    public readonly NativePersistencePlayerState State = state;
}
