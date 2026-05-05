using Octaryn.Shared.Time;

namespace Octaryn.Server.World.Time;

internal sealed unsafe class WorldTimeClock : IDisposable
{
    private IntPtr _handle;

    public WorldTimeClock()
        : this(WorldTimeConfig.Default)
    {
    }

    public WorldTimeClock(WorldTimeConfig config)
    {
        _handle = NativeWorldTimeLibrary.ClockCreate();
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Native server world-time clock allocation failed.");
        }

        Reset(config);
    }

    ~WorldTimeClock()
    {
        Dispose();
    }

    public ulong DayIndex => NativeWorldTimeLibrary.ClockDayIndex(Handle);

    public double SecondsOfDay => NativeWorldTimeLibrary.ClockSecondsOfDay(Handle);

    public void Reset(WorldTimeConfig? config = null)
    {
        var nativeConfig = NativeWorldTimeConfig.FromWorldTimeConfig(config ?? WorldTimeConfig.Default);
        NativeWorldTimeLibrary.ClockReset(Handle, &nativeConfig);
    }

    public WorldTime AdvanceFrame(double deltaSeconds)
    {
        return NativeWorldTimeLibrary.ClockAdvanceFrame(Handle, deltaSeconds).ToWorldTime();
    }

    public void SetSpeedMultiplier(double multiplier)
    {
        NativeWorldTimeLibrary.ClockSetSpeedMultiplier(Handle, multiplier);
    }

    public void AdvanceRealSeconds(double realSeconds)
    {
        NativeWorldTimeLibrary.ClockAdvance(Handle, realSeconds);
    }

    public WorldTimeSnapshot Snapshot()
    {
        return NativeWorldTimeLibrary.ClockSnapshot(Handle).ToWorldTimeSnapshot();
    }

    public WorldTimeBlob WriteBlob()
    {
        return NativeWorldTimeLibrary.ClockWriteBlob(Handle).ToWorldTimeBlob();
    }

    public bool TryReadBlob(WorldTimeConfig config, WorldTimeBlob blob)
    {
        var nativeConfig = NativeWorldTimeConfig.FromWorldTimeConfig(config);
        var nativeBlob = NativeWorldTimeBlob.FromWorldTimeBlob(blob);
        return NativeWorldTimeLibrary.ClockReadBlob(Handle, &nativeConfig, &nativeBlob) != 0;
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
