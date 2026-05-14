using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.World.Time;

return WorldTimeProbe.Run();

internal static unsafe class WorldTimeProbe
{
    private const double WorldSecondsPerDay = 24.0 * 60.0 * 60.0;
    private const uint WorldTimeVersion = 1;

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
        var clock = CreateNativeClock(new NativeWorldTimeConfig(1800.0, 2000, 2, 28, 0.0));
        try
        {
            NativeWorldTimeLibrary.ClockAdvance(clock, 1800.0);
            var leapSnapshot = NativeWorldTimeLibrary.ClockSnapshot(clock).ToWorldTimeSnapshot();
            Require(leapSnapshot.Date.Month == 2, "leap February month");
            Require(leapSnapshot.Date.Day == 29, "leap February day");

            ResetNativeClock(clock, new NativeWorldTimeConfig(1800.0, 1900, 2, 28, 0.0));
            NativeWorldTimeLibrary.ClockAdvance(clock, 1800.0);
            var nonLeapSnapshot = NativeWorldTimeLibrary.ClockSnapshot(clock).ToWorldTimeSnapshot();
            Require(nonLeapSnapshot.Date.Month == 3, "non-leap March month");
            Require(nonLeapSnapshot.Date.Day == 1, "non-leap March day");

            ResetNativeClock(clock, new NativeWorldTimeConfig(1800.0, 1000, 13, 99, 0.0));
            var invalidStartSnapshot = NativeWorldTimeLibrary.ClockSnapshot(clock).ToWorldTimeSnapshot();
            Require(invalidStartSnapshot.Date.Month == 1, "invalid month fallback");
            Require(invalidStartSnapshot.Date.Day == 31, "invalid day clamp");
        }
        finally
        {
            NativeWorldTimeLibrary.ClockDestroy(clock);
        }
    }

    private static void ValidateBlobRead()
    {
        var clock = CreateNativeClock(DefaultNativeConfig());
        try
        {
            var loaded = TryReadNativeBlob(
                clock,
                DefaultNativeConfig(),
                new NativeWorldTimeBlob(WorldTimeVersion, 2, WorldSecondsPerDay * 2.0 + 12.5));
            Require(loaded, "blob load");
            Require(NativeWorldTimeLibrary.ClockDayIndex(clock) == 4, "blob day carry");
            Require(Math.Abs(NativeWorldTimeLibrary.ClockSecondsOfDay(clock) - 12.5) < 0.0001, "blob seconds");

            loaded = TryReadNativeBlob(clock, DefaultNativeConfig(), new NativeWorldTimeBlob(99, 0, 0.0));
            Require(!loaded, "reject unknown blob version");
        }
        finally
        {
            NativeWorldTimeLibrary.ClockDestroy(clock);
        }
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

        var expected = new ProbeWorldTimeState(WorldTimeVersion, 7, 123.25);
        SaveWorldTime(path, expected);
        Require(TryLoadWorldTime(path, out var actual), "world-time native persistence load");
        Require(actual == expected, "world-time native persistence round trip");
        Require(File.ReadAllText(path).Contains("\"seconds_of_day\"", StringComparison.Ordinal), "world-time JSON shape");
    }

    private static bool TryLoadWorldTime(string path, out ProbeWorldTimeState blob)
    {
        blob = default;
        if (!NativeWorldPersistenceLibrary.TryReadWorldTimeFile(path, out var state) ||
            state.Version != WorldTimeVersion)
        {
            return false;
        }

        blob = new ProbeWorldTimeState(state.Version, state.DayIndex, state.SecondsOfDay);
        return true;
    }

    private static void SaveWorldTime(string path, ProbeWorldTimeState blob)
    {
        NativeWorldPersistenceLibrary.WriteWorldTimeFile(
            path,
            new NativePersistenceWorldTimeState(blob.Version, blob.DayIndex, blob.SecondsOfDay));
    }

    private static NativeWorldTimeConfig DefaultNativeConfig()
    {
        return new NativeWorldTimeConfig(1800.0, 1000, 1, 1, 12.0 * 60.0 * 60.0);
    }

    private static IntPtr CreateNativeClock(NativeWorldTimeConfig config)
    {
        var clock = NativeWorldTimeLibrary.ClockCreate();
        if (clock == IntPtr.Zero)
        {
            throw new InvalidOperationException("Native server world-time clock allocation failed.");
        }

        ResetNativeClock(clock, config);
        return clock;
    }

    private static void ResetNativeClock(IntPtr clock, NativeWorldTimeConfig config)
    {
        NativeWorldTimeLibrary.ClockReset(clock, &config);
    }

    private static bool TryReadNativeBlob(IntPtr clock, NativeWorldTimeConfig config, NativeWorldTimeBlob blob)
    {
        return NativeWorldTimeLibrary.ClockReadBlob(clock, &config, &blob) != 0;
    }

    private static void Require(bool condition, string label)
    {
        if (!condition)
        {
            throw new InvalidOperationException($"World-time probe failed: {label}.");
        }
    }
}

internal readonly record struct ProbeWorldTimeState(
    uint Version,
    ulong DayIndex,
    double SecondsOfDay);
