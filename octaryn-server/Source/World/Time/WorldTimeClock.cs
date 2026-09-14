using Octaryn.Shared.Time;

namespace Octaryn.Server.World.Time;

internal sealed unsafe class WorldTimeClock : IDisposable
{
    private IntPtr _handle;
    private int _hourOffset;

    public WorldTimeClock()
    {
        _handle = NativeWorldTimeLibrary.ClockCreate();
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Native server world-time clock allocation failed.");
        }

        var startHour = Environment.GetEnvironmentVariable("OCTARYN_SERVER_START_HOUR");
        if (!string.IsNullOrWhiteSpace(startHour))
        {
            if (!double.TryParse(startHour, System.Globalization.NumberStyles.Float,
                    System.Globalization.CultureInfo.InvariantCulture, out var hour) ||
                !double.IsFinite(hour) || hour < 0 || hour >= 24)
                throw new ArgumentException("OCTARYN_SERVER_START_HOUR must be in [0, 24).");
            var config = new NativeWorldTimeConfig(1800, 1000, 1, 1, hour * 3600);
            NativeWorldTimeLibrary.ClockReset(Handle, &config);
        }
        else NativeWorldTimeLibrary.ClockReset(Handle, null);
    }

    ~WorldTimeClock()
    {
        Dispose();
    }

    public WorldTime AdvanceFrame(double deltaSeconds)
    {
        return NativeWorldTimeLibrary.ClockAdvanceFrame(Handle, deltaSeconds).ToWorldTime();
    }

    public void SetSpeedMultiplier(double multiplier)
    {
        NativeWorldTimeLibrary.ClockSetSpeedMultiplier(Handle, multiplier);
    }

    public void SetHourOffset(int offset)
    {
        NativeWorldTimeLibrary.ClockStepHours(Handle, offset - _hourOffset);
        _hourOffset = offset;
    }

    public WorldTimeSnapshot Snapshot()
    {
        return NativeWorldTimeLibrary.ClockSnapshot(Handle).ToWorldTimeSnapshot();
    }

    public void Dispose()
    {
        var handle = _handle;
        if (handle == IntPtr.Zero)
        {
            return;
        }

        _handle = IntPtr.Zero;
        NativeWorldTimeLibrary.ClockDestroy(handle);
        GC.SuppressFinalize(this);
    }

    private IntPtr Handle
    {
        get
        {
            ObjectDisposedException.ThrowIf(_handle == IntPtr.Zero, this);
            return _handle;
        }
    }
}
