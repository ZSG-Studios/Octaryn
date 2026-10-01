using System.Diagnostics;
using System.Globalization;

namespace Octaryn.Server.Tick;

// Complete listener pump, including network callbacks, module ticks and replication.
internal sealed class AuthorityLoopProfile : IDisposable
{
    private readonly BoundedLogWriter? _writer;
    private long _start, _phase, _allocated;
    private long _origin;
    private CpuExecutionSample _cpu;
    private double _control, _network, _entities;
    private ulong _sequence;

    public AuthorityLoopProfile()
    {
        var path = Environment.GetEnvironmentVariable("OCTARYN_SERVER_LOOP_PROFILE");
        if (string.IsNullOrWhiteSpace(path)) return;
        _writer = new BoundedLogWriter(path, capacity: 8192);
        _writer.TryWrite("schema,pump,authority_tick,control_wall_ms,network_wall_ms,entity_wall_ms,authority_session_wall_ms,total_wall_ms,process_alloc_bytes,elapsed_wall_ms,thread_cycles,thread_cycles_valid,gc_pause_ms");
    }

    public void Begin()
    {
        if (_writer is null) return;
        _allocated = GC.GetTotalAllocatedBytes(false);
        _cpu = CpuExecutionSample.Read();
        _start = _phase = Stopwatch.GetTimestamp();
        if (_origin == 0) _origin = _start;
    }

    public void ControlDone()
    {
        if (_writer is null) return;
        var now = Stopwatch.GetTimestamp();
        _control = (now - _phase) * 1000.0 / Stopwatch.Frequency;
        _phase = now;
    }

    public void NetworkDone()
    {
        if (_writer is null) return;
        var now = Stopwatch.GetTimestamp();
        _network = (now - _phase) * 1000.0 / Stopwatch.Frequency;
        _phase = now;
    }

    public void EntitiesDone()
    {
        if (_writer is null) return;
        var now = Stopwatch.GetTimestamp();
        _entities = (now - _phase) * 1000.0 / Stopwatch.Frequency;
        _phase = now;
    }

    public void End(ulong tick)
    {
        if (_writer is null) return;
        var now = Stopwatch.GetTimestamp();
        var bytes = GC.GetTotalAllocatedBytes(false) - _allocated;
        var cpu = CpuExecutionSample.Read();
        var scale = 1000.0 / Stopwatch.Frequency;
        _writer.TryWrite(string.Create(CultureInfo.InvariantCulture,
            $"2,{++_sequence},{tick},{_control:F6},{_network:F6},{_entities:F6},{(now-_phase)*scale:F6},{(now-_start)*scale:F6},{bytes},{(now-_origin)*scale:F6},{cpu.CyclesSince(_cpu)},{(cpu.CyclesValid && _cpu.CyclesValid ? 1 : 0)},{cpu.GcPauseMillisecondsSince(_cpu):F6}"));
    }

    public void Dispose() => _writer?.Dispose();
}
