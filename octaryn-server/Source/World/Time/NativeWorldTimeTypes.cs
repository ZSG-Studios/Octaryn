using System.Runtime.InteropServices;
using Octaryn.Shared.Time;

namespace Octaryn.Server.World.Time;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeWorldTimeDate(int year, int month, int day)
{
    public readonly int Year = year;
    public readonly int Month = month;
    public readonly int Day = day;

    public WorldDate ToWorldDate()
    {
        return new WorldDate(Year, Month, Day);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeWorldTimeConfig(
    double realSecondsPerDay,
    int startYear,
    int startMonth,
    int startDay,
    double startSecondsOfDay)
{
    public readonly double RealSecondsPerDay = realSecondsPerDay;
    public readonly int StartYear = startYear;
    public readonly int StartMonth = startMonth;
    public readonly int StartDay = startDay;
    public readonly double StartSecondsOfDay = startSecondsOfDay;

    public static NativeWorldTimeConfig FromWorldTimeConfig(WorldTimeConfig config)
    {
        return new NativeWorldTimeConfig(
            config.RealSecondsPerDay,
            config.StartYear,
            config.StartMonth,
            config.StartDay,
            config.StartSecondsOfDay);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeWorldTimeSnapshot(
    NativeWorldTimeDate date,
    ulong dayIndex,
    uint secondOfDay,
    uint hour,
    uint minute,
    uint second,
    double totalWorldSeconds,
    float dayFraction)
{
    public readonly NativeWorldTimeDate Date = date;
    public readonly ulong DayIndex = dayIndex;
    public readonly uint SecondOfDay = secondOfDay;
    public readonly uint Hour = hour;
    public readonly uint Minute = minute;
    public readonly uint Second = second;
    public readonly double TotalWorldSeconds = totalWorldSeconds;
    public readonly float DayFraction = dayFraction;

    public WorldTimeSnapshot ToWorldTimeSnapshot()
    {
        return new WorldTimeSnapshot(
            Date.ToWorldDate(),
            DayIndex,
            SecondOfDay,
            Hour,
            Minute,
            Second,
            TotalWorldSeconds,
            DayFraction);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeWorldTimeBlob(uint version, ulong dayIndex, double secondsOfDay)
{
    public readonly uint Version = version;
    public readonly ulong DayIndex = dayIndex;
    public readonly double SecondsOfDay = secondsOfDay;

    public static NativeWorldTimeBlob FromWorldTimeBlob(WorldTimeBlob blob)
    {
        return new NativeWorldTimeBlob(blob.Version, blob.DayIndex, blob.SecondsOfDay);
    }

    public WorldTimeBlob ToWorldTimeBlob()
    {
        return new WorldTimeBlob(Version, DayIndex, SecondsOfDay);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeWorldTimeFrame(
    ulong tickId,
    ulong dayIndex,
    double deltaSeconds,
    double totalSeconds)
{
    public readonly ulong TickId = tickId;
    public readonly ulong DayIndex = dayIndex;
    public readonly double DeltaSeconds = deltaSeconds;
    public readonly double TotalSeconds = totalSeconds;

    public WorldTime ToWorldTime()
    {
        return new WorldTime(TickId, DayIndex, DeltaSeconds, TotalSeconds);
    }
}
