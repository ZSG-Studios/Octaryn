using System.Runtime.InteropServices;

namespace Octaryn.Engine.Api;

[StructLayout(LayoutKind.Sequential, Size = 56)]
public readonly struct OctarynInputSnapshot
{
    public const uint VersionValue = 1;
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

    public bool Jump => (Flags & JumpFlag) != 0;
    public bool Sprint => (Flags & SprintFlag) != 0;
    public bool FlyMode => (Flags & FlyModeFlag) != 0;
    public bool Primary => (Flags & PrimaryFlag) != 0;
    public bool Secondary => (Flags & SecondaryFlag) != 0;
}
