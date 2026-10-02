namespace Octaryn.Shared.Host.Api;

// Public module-facing copy of the latest input state. Flag bit meanings
// match HostInputSnapshot (jump, sprint, fly, primary, secondary, menu edge).
public readonly record struct HostInputState(
    uint Flags,
    uint Controller,
    float MoveX,
    float MoveY,
    float MoveZ,
    float CameraPitch,
    float CameraYaw)
{
    public bool Jump => (Flags & (1u << 0)) != 0;

    public bool Sprint => (Flags & (1u << 1)) != 0;

    public bool FlyMode => (Flags & (1u << 2)) != 0;

    public bool Primary => (Flags & (1u << 3)) != 0;

    public bool Secondary => (Flags & (1u << 4)) != 0;
    public bool MenuPressed => (Flags & (1u << 5)) != 0;
    public bool UsePressed => (Flags & (1u << 6)) != 0;
    public bool GrabPressed => (Flags & (1u << 7)) != 0;
}
