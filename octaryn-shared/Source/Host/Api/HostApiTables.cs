using System.Runtime.InteropServices;

namespace Octaryn.Shared.Host.Api;

// Unsafe mirrors of octaryn_host_api.h. Layout is pinned by _Static_assert
// on the C side; keep field order identical.
[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 32)]
internal unsafe struct HostTimeApiTable
{
    public const uint VersionValue = 1;
    public const uint SizeValue = 32;

    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<double> NowSeconds;
    public delegate* unmanaged[Cdecl]<ulong> TickId;
    public delegate* unmanaged[Cdecl]<double> TickRate;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 16)]
internal unsafe struct HostDiagnosticsApiTable
{
    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<uint, byte*, void> LogWrite;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 40)]
internal unsafe struct HostRaycastHitNative
{
    public uint Hit;
    public uint MaterialId;
    public float PointX;
    public float PointY;
    public float PointZ;
    public float NormalX;
    public float NormalY;
    public float NormalZ;
    public float Distance;
    public uint TriangleIndex;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 48)]
internal struct HostCharacterInputNative
{
    public uint Flags;
    public uint Controller;
    public float MoveX;
    public float MoveY;
    public float MoveZ;
    public float CameraX;
    public float CameraY;
    public float CameraZ;
    public float CameraPitch;
    public float CameraYaw;
    public int RelativeMouse;
    public uint Reserved;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 48)]
internal struct HostCharacterStateNative
{
    public float X;
    public float Y;
    public float Z;
    public float Pitch;
    public float Yaw;
    public float VelocityX;
    public float VelocityY;
    public float VelocityZ;
    public uint IsOnGround;
    public uint ControlMode;
    public uint JumpHeld;
    public uint Reserved;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
internal unsafe struct HostPhysicsApiTable
{
    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<float, float, float, float, float, float, float, HostRaycastHitNative*, int> Raycast;
    public delegate* unmanaged[Cdecl]<HostCharacterInputNative*, double, HostCharacterStateNative*, int> MoveCharacter;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
internal struct HostSpawnPoseNative
{
    public float X;
    public float Y;
    public float Z;
    public float Yaw;
    public float Pitch;
    public uint Valid;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 32)]
internal unsafe struct HostWorldApiTable
{
    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<HostSpawnPoseNative*, int> SpawnPose;
    public delegate* unmanaged[Cdecl]<ulong> TriangleCount;
    public delegate* unmanaged[Cdecl]<int> IsActive;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 16)]
internal unsafe struct HostInputApiTable
{
    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<HostInputSnapshot*, int> PollInput;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 16)]
internal unsafe struct HostSchedulingApiTable
{
    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<uint, delegate* unmanaged[Cdecl]<void*, void>, void*, int> SubmitWork;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 16)]
internal unsafe struct HostAudioApiTable
{
    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<ulong, float, float, float, float, int> PlayActionSound;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
internal unsafe struct HostUiApiTable
{
    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<byte*, int> ShowNotification;
    public delegate* unmanaged[Cdecl]<byte*, uint, int> PollUiAction;
}

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
internal unsafe struct HostReplicationApiTable
{
    public uint Version;
    public uint Size;
    public delegate* unmanaged[Cdecl]<Networking.ReplicationChange*, int> PublishChange;
    public delegate* unmanaged[Cdecl]<ulong, byte*, uint, int> SendMessage;
}
