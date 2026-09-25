namespace Octaryn.Shared.Host;

[Flags]
public enum HostWorkScheduleFlags
{
    None = 0,
    DeterministicOrder = 1 << 0,
    CanRunInParallel = 1 << 1,
    RequiresTickBarrier = 1 << 2
}
