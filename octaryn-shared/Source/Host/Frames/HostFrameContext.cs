namespace Octaryn.Shared.Host;

internal readonly record struct HostFrameContext(
    double DeltaSeconds,
    ulong FrameIndex,
    HostInputSnapshot Input)
{
    public static HostFrameContext FromSnapshot(in HostFrameSnapshot snapshot)
    {
        return new HostFrameContext(
            snapshot.Timing.DeltaSeconds,
            snapshot.Timing.FrameIndex,
            snapshot.Input);
    }
}
