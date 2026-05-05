using System.Runtime.InteropServices;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Simulation.Players;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeInput(
    uint flags,
    uint controller,
    float moveX,
    float moveY,
    float moveZ,
    float cameraX,
    float cameraY,
    float cameraZ,
    float cameraPitch,
    float cameraYaw,
    int relativeMouse)
{
    public readonly uint Flags = flags;
    public readonly uint Controller = controller;
    public readonly float MoveX = moveX;
    public readonly float MoveY = moveY;
    public readonly float MoveZ = moveZ;
    public readonly float CameraX = cameraX;
    public readonly float CameraY = cameraY;
    public readonly float CameraZ = cameraZ;
    public readonly float CameraPitch = cameraPitch;
    public readonly float CameraYaw = cameraYaw;
    public readonly int RelativeMouse = relativeMouse;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeInputIntent(
    int version,
    ulong frameIndex,
    double deltaSeconds,
    NativeInput input)
{
    public readonly int Version = version;
    public readonly ulong FrameIndex = frameIndex;
    public readonly double DeltaSeconds = deltaSeconds;
    public readonly NativeInput Input = input;

    public HostFrameSnapshot ToFrameSnapshot()
    {
        return new HostFrameSnapshot(
            new HostInputSnapshot(
                HostInputSnapshot.VersionValue,
                HostInputSnapshot.SizeValue,
                Input.Flags,
                Input.Controller,
                Input.MoveX,
                Input.MoveY,
                Input.MoveZ,
                Input.CameraX,
                Input.CameraY,
                Input.CameraZ,
                Input.CameraPitch,
                Input.CameraYaw,
                Input.RelativeMouse),
            new HostFrameTimingSnapshot(
                HostFrameTimingSnapshot.VersionValue,
                HostFrameTimingSnapshot.SizeValue,
                FrameIndex,
                DeltaSeconds));
    }
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeState(
    float x,
    float y,
    float z,
    float pitch,
    float yaw,
    float velocityX,
    float velocityY,
    float velocityZ,
    uint isOnGround,
    uint controlMode,
    ushort selectedBlock)
{
    public float X = x;
    public float Y = y;
    public float Z = z;
    public float Pitch = pitch;
    public float Yaw = yaw;
    public float VelocityX = velocityX;
    public float VelocityY = velocityY;
    public float VelocityZ = velocityZ;
    public uint IsOnGround = isOnGround;
    public uint ControlMode = controlMode;
    public ushort SelectedBlock = selectedBlock;
    public ushort Reserved;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeSaveState(
    float x,
    float y,
    float z,
    float pitch,
    float yaw,
    ushort selectedBlock)
{
    public float X = x;
    public float Y = y;
    public float Z = z;
    public float Pitch = pitch;
    public float Yaw = yaw;
    public ushort SelectedBlock = selectedBlock;
    public ushort Reserved;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeSaveDecision
{
    public uint ShouldSave;
    public uint Reserved;
    public double SecondsSinceLastSave;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeSpawnAlignment
{
    public uint Aligned;
    public uint Adjusted;
    public int SurfaceY;
    public ushort SurfaceBlock;
    public ushort Reserved;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeTickResult
{
    public uint TickInput;
    public uint Reserved;
    public float DeltaX;
    public float DeltaY;
    public float DeltaZ;
}
