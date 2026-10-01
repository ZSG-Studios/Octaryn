namespace Octaryn.Basegame.Gameplay.Time;

// Authoritative world clock mirrored into the ECS world for gameplay rules.
public struct WorldClockComponent
{
    public ulong TickId;
    public double TotalSeconds;
    public int LastHour = -1;

    public WorldClockComponent()
    {
    }

    // Matches the host clock convention (octaryn-server Clock.h).
    public const double WorldSecondsPerDay = 24.0 * 60.0 * 60.0;

    public readonly double DayFraction01
    {
        get
        {
            var wrapped = TotalSeconds % WorldSecondsPerDay;
            if (wrapped < 0.0)
            {
                wrapped += WorldSecondsPerDay;
            }

            return wrapped / WorldSecondsPerDay;
        }
    }

    // Sine of the sun's elevation angle, matching the sky orbit convention.
    public readonly float SunElevation
    {
        get
        {
            var orbit = (float)(DayFraction01 * 2.0 * System.Math.PI - System.Math.PI / 2.0);
            return System.MathF.Sin(orbit);
        }
    }

    public readonly bool IsNight => SunElevation < -0.08f;
}
