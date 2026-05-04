namespace Octaryn.Basegame.Gameplay.Rules;

public readonly record struct FrameState(
    double DeltaSeconds,
    ulong FrameIndex);
