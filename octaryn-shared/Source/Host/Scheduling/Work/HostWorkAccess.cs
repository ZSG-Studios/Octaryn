namespace Octaryn.Shared.Host;

[Flags]
internal enum HostWorkAccess
{
    None = 0,
    InputSnapshot = 1 << 0,
    FrameTimingSnapshot = 1 << 1,
    GameplayStateRead = 1 << 2,
    GameplayStateWrite = 1 << 3,
    ContentRegistryRead = 1 << 4,
    CommandSinkWrite = 1 << 5
}
