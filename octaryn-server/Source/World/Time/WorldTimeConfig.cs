namespace Octaryn.Server.World.Time;

internal readonly record struct WorldTimeConfig(
    double RealSecondsPerDay,
    int StartYear,
    int StartMonth,
    int StartDay,
    double StartSecondsOfDay)
{
    public const double WorldSecondsPerDay = 24.0 * 60.0 * 60.0;

    public static WorldTimeConfig Default { get; } = new(
        1800.0,
        1000,
        1,
        1,
        12.0 * 60.0 * 60.0);
}
