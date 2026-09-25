using Octaryn.Shared.Time;

namespace Octaryn.Shared.GameModules;

public readonly record struct ModuleFrameContext
{
    public ModuleFrameContext(double deltaSeconds, ulong frameIndex)
        : this(deltaSeconds, frameIndex, default)
    {
    }

    public ModuleFrameContext(double deltaSeconds, ulong frameIndex, WorldTime worldTime)
    {
        DeltaSeconds = deltaSeconds;
        FrameIndex = frameIndex;
        WorldTime = worldTime;
    }

    public double DeltaSeconds { get; }

    public ulong FrameIndex { get; }

    public WorldTime WorldTime { get; }
}
