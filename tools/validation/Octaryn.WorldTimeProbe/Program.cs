using Octaryn.Server.Persistence.WorldTime;
using Octaryn.Server.World.Time;

return WorldTimeProbe.Run();

internal static class WorldTimeProbe
{
    public static int Run()
    {
        ValidateDefaultSnapshot();
        ValidateAdvanceAndDateCarry();
        ValidateSpeedMultiplier();
        ValidateCalendar();
        ValidateBlobRead();
        ValidateStoreRoundTrip();
        return 0;
    }

    private static void ValidateDefaultSnapshot()
    {
        using var clock = new WorldTimeClock();
        var snapshot = clock.Snapshot();
        Require(snapshot.Date.Year == 1000, "default year");
        Require(snapshot.Date.Month == 1, "default month");
        Require(snapshot.Date.Day == 1, "default day");
        Require(snapshot.DayIndex == 0, "default day index");
        Require(snapshot.SecondOfDay == 43200, "default second of day");
        Require(snapshot.Hour == 12, "default hour");
        Require(Math.Abs(snapshot.DayFraction - 0.5f) < 0.0001f, "default day fraction");
    }

    private static void ValidateAdvanceAndDateCarry()
    {
        using var clock = new WorldTimeClock();
        var worldTime = clock.AdvanceFrame(900.0);
        var snapshot = clock.Snapshot();
        Require(worldTime.TickId == 0, "first world-time tick id");
        Require(worldTime.DayIndex == 1, "world-time day carry");
        Require(Math.Abs(worldTime.DeltaSeconds - 900.0) < 0.0001, "world-time delta");
        Require(snapshot.Date.Year == 1000, "advanced year");
        Require(snapshot.Date.Month == 1, "advanced month");
        Require(snapshot.Date.Day == 2, "advanced day");
        Require(snapshot.SecondOfDay == 0, "advanced second of day");
        Require(snapshot.Hour == 0, "advanced hour");
        Require(Math.Abs(snapshot.TotalWorldSeconds - 86400.0) < 0.0001, "advanced total world seconds");
    }

    private static void ValidateSpeedMultiplier()
    {
        using var clock = new WorldTimeClock();
        clock.SetSpeedMultiplier(2.0);
        var worldTime = clock.AdvanceFrame(0.5);
        var snapshot = clock.Snapshot();
        Require(Math.Abs(worldTime.DeltaSeconds - 1.0) < 0.0001, "speed multiplier delta");
        Require(Math.Abs(snapshot.TotalWorldSeconds - 43248.0) < 0.0001, "speed multiplier total world seconds");

        clock.SetSpeedMultiplier(0.0);
        worldTime = clock.AdvanceFrame(10.0);
        snapshot = clock.Snapshot();
        Require(Math.Abs(worldTime.DeltaSeconds) < 0.0001, "speed multiplier pause delta");
        Require(Math.Abs(snapshot.TotalWorldSeconds - 43248.0) < 0.0001, "speed multiplier pause total world seconds");
    }

    private static void ValidateCalendar()
    {
        using var clock = new WorldTimeClock(new WorldTimeConfig(1800.0, 2000, 2, 28, 0.0));
        clock.AdvanceRealSeconds(1800.0);
        var leapSnapshot = clock.Snapshot();
        Require(leapSnapshot.Date.Month == 2, "leap February month");
        Require(leapSnapshot.Date.Day == 29, "leap February day");

        clock.Reset(new WorldTimeConfig(1800.0, 1900, 2, 28, 0.0));
        clock.AdvanceRealSeconds(1800.0);
        var nonLeapSnapshot = clock.Snapshot();
        Require(nonLeapSnapshot.Date.Month == 3, "non-leap March month");
        Require(nonLeapSnapshot.Date.Day == 1, "non-leap March day");

        clock.Reset(new WorldTimeConfig(1800.0, 1000, 13, 99, 0.0));
        var invalidStartSnapshot = clock.Snapshot();
        Require(invalidStartSnapshot.Date.Month == 1, "invalid month fallback");
        Require(invalidStartSnapshot.Date.Day == 31, "invalid day clamp");
    }

    private static void ValidateBlobRead()
    {
        using var clock = new WorldTimeClock();
        var loaded = clock.TryReadBlob(
            WorldTimeConfig.Default,
            new WorldTimeBlob(WorldTimeBlob.CurrentVersion, 2, WorldTimeConfig.WorldSecondsPerDay * 2.0 + 12.5));
        Require(loaded, "blob load");
        Require(clock.DayIndex == 4, "blob day carry");
        Require(Math.Abs(clock.SecondsOfDay - 12.5) < 0.0001, "blob seconds");

        loaded = clock.TryReadBlob(WorldTimeConfig.Default, new WorldTimeBlob(99, 0, 0.0));
        Require(!loaded, "reject unknown blob version");
    }

    private static void ValidateStoreRoundTrip()
    {
        var root = Environment.GetEnvironmentVariable("OCTARYN_WORLD_TIME_PROBE_DIR");
        if (string.IsNullOrWhiteSpace(root))
        {
            root = Path.Combine(Path.GetTempPath(), "octaryn-world-time-probe");
        }

        Directory.CreateDirectory(root);
        var path = Path.Combine(root, "world_time.json");
        if (File.Exists(path))
        {
            File.Delete(path);
        }

        var expected = new WorldTimeBlob(WorldTimeBlob.CurrentVersion, 7, 123.25);
        WorldTimeStore.Save(path, expected);
        Require(WorldTimeStore.TryLoad(path, out var actual), "world-time store load");
        Require(actual == expected, "world-time store round trip");
        Require(File.ReadAllText(path).Contains("\"seconds_of_day\"", StringComparison.Ordinal), "world-time JSON shape");
    }

    private static void Require(bool condition, string label)
    {
        if (!condition)
        {
            throw new InvalidOperationException($"World-time probe failed: {label}.");
        }
    }
}
