using System.Diagnostics;
using System.Globalization;

namespace Octaryn.Server.Tick;

// Opt-in phase wall time and allocations, collected on the executing worker.
// This is execution cost, not the configured simulation timestep.
internal sealed class AuthorityTickProfile : IDisposable
{
    private readonly BoundedLogWriter? _writer;
    private readonly long[] _ticks = new long[3];
    private readonly long[] _bytes = new long[3];
    private readonly ulong[] _cycles = new ulong[3];
    private CpuExecutionSample _cpu, _scheduleCpu;
    private long _start, _allocated;

    public AuthorityTickProfile()
    {
        var path = Environment.GetEnvironmentVariable("OCTARYN_SERVER_TICK_PROFILE");
        if (string.IsNullOrWhiteSpace(path)) return;
        _writer = new BoundedLogWriter(path, capacity: 8192);
        _writer.TryWrite("frame,command_wall_ms,player_wall_ms,time_wall_ms,command_alloc_bytes,player_alloc_bytes,time_alloc_bytes,schedule_wall_ms,command_thread_cycles,player_thread_cycles,time_thread_cycles,schedule_thread_cycles,thread_cycles_valid,schedule_gc_pause_ms");
    }

    public void Begin()
    {
        if (_writer is null) return;
        _allocated = GC.GetAllocatedBytesForCurrentThread();
        _cpu = CpuExecutionSample.Read();
        _start = Stopwatch.GetTimestamp();
    }

    public void End(int phase)
    {
        if (_writer is null) return;
        _ticks[phase] = Stopwatch.GetTimestamp() - _start;
        _bytes[phase] = GC.GetAllocatedBytesForCurrentThread() - _allocated;
        _cycles[phase] = CpuExecutionSample.Read().CyclesSince(_cpu);
    }

    public void BeginSchedule() { if (_writer is not null) _scheduleCpu = CpuExecutionSample.Read(); }

    public void Publish(ulong frame, long start)
    {
        if (_writer is null) return;
        var scale = 1000.0 / Stopwatch.Frequency;
        var cpu = CpuExecutionSample.Read();
        _writer.TryWrite(string.Create(CultureInfo.InvariantCulture,
            $"{frame},{_ticks[0]*scale:F6},{_ticks[1]*scale:F6},{_ticks[2]*scale:F6},{_bytes[0]},{_bytes[1]},{_bytes[2]},{(Stopwatch.GetTimestamp()-start)*scale:F6},{_cycles[0]},{_cycles[1]},{_cycles[2]},{cpu.CyclesSince(_scheduleCpu)},{(cpu.CyclesValid && _scheduleCpu.CyclesValid ? 1 : 0)},{cpu.GcPauseMillisecondsSince(_scheduleCpu):F6}"));
    }

    public void Dispose() => _writer?.Dispose();
}
