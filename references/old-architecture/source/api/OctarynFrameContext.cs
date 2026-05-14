namespace Octaryn.Engine.Api;

public readonly record struct OctarynFrameContext(
    double DeltaSeconds,
    ulong FrameIndex,
    OctarynInputSnapshot Input,
    OctarynWorldSnapshot World)
{
    public static OctarynFrameContext FromSnapshot(in OctarynFrameSnapshot snapshot)
    {
        return new OctarynFrameContext(
            snapshot.World.DeltaSeconds,
            snapshot.World.FrameIndex,
            snapshot.Input,
            snapshot.World);
    }
}
