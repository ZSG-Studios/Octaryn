using Octaryn.Shared.Time;

namespace Octaryn.Server.World.Time;

internal sealed unsafe class WorldTimeClock : IDisposable
{
    private IntPtr _handle;

    public WorldTimeClock()
    {
        _handle = NativeWorldTimeLibrary.ClockCreate();
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Native server world-time clock allocation failed.");
        }

        NativeWorldTimeLibrary.ClockReset(Handle, null);
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
