namespace Octaryn.Shared.Time;

public readonly record struct WorldDate(
    int Year,
    int Month,
    int Day);

public readonly record struct WorldTime(
    ulong TickId,
    ulong DayIndex,
    double DeltaSeconds,
    double TotalSeconds);

public readonly record struct WorldTimeSnapshot(
    WorldDate Date,
    ulong DayIndex,
    uint SecondOfDay,
    uint Hour,
    uint Minute,
    uint Second,
    double TotalWorldSeconds,
    float DayFraction);
