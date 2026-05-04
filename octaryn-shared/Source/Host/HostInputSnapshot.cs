using System.Runtime.InteropServices;

namespace Octaryn.Shared.Host;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 56)]
internal readonly struct HostInputSnapshot
{
    public const uint VersionValue = 1;
    public const uint SizeValue = 56;
    public const uint JumpFlag = 1u << 0;
    public const uint SprintFlag = 1u << 1;
    public const uint FlyModeFlag = 1u << 2;
    public const uint PrimaryFlag = 1u << 3;
    public const uint SecondaryFlag = 1u << 4;

    public readonly uint Version;
    public readonly uint Size;
    public readonly uint Flags;
    public readonly uint Controller;
    public readonly float MoveX;
    public readonly float MoveY;
    public readonly float MoveZ;
    public readonly float CameraX;
    public readonly float CameraY;
    public readonly float CameraZ;
    public readonly float CameraPitch;
    public readonly float CameraYaw;
    public readonly int RelativeMouse;
    public readonly int Reserved;

    internal HostInputSnapshot(uint version, uint size)
    {
        Version = version;
        Size = size;
        Flags = 0;
        Controller = 0;
        MoveX = 0;
        MoveY = 0;
        MoveZ = 0;
        CameraX = 0;
        CameraY = 0;
        CameraZ = 0;
        CameraPitch = 0;
        CameraYaw = 0;
        RelativeMouse = 0;
        Reserved = 0;
    }

    internal HostInputSnapshot(
        uint version,
        uint size,
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
        Version = version;
        Size = size;
        Flags = flags;
        Controller = controller;
        MoveX = moveX;
        MoveY = moveY;
        MoveZ = moveZ;
        CameraX = cameraX;
        CameraY = cameraY;
        CameraZ = cameraZ;
        CameraPitch = cameraPitch;
        CameraYaw = cameraYaw;
        RelativeMouse = relativeMouse;
        Reserved = 0;
    }

    public bool Jump => (Flags & JumpFlag) != 0;
    public bool Sprint => (Flags & SprintFlag) != 0;
    public bool FlyMode => (Flags & FlyModeFlag) != 0;
    public bool Primary => (Flags & PrimaryFlag) != 0;
    public bool Secondary => (Flags & SecondaryFlag) != 0;
}
