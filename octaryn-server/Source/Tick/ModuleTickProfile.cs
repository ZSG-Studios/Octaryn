using System.Diagnostics;
using System.Globalization;

namespace Octaryn.Server.Tick;

internal sealed class ModuleTickProfile : IDisposable
{
    private readonly BoundedLogWriter? _writer;
    private long _start, _allocated;
    private CpuExecutionSample _cpu;
    public ModuleTickProfile()
    {
        var path = Environment.GetEnvironmentVariable("OCTARYN_SERVER_MODULE_PROFILE");
        if (string.IsNullOrWhiteSpace(path)) return;
        _writer = new BoundedLogWriter(path, capacity: 8192);
        _writer.TryWrite("schema,frame,authority_schedule_ms,module_schedule_ms,whole_tick_ms,process_alloc_bytes,thread_cycles,thread_cycles_valid,gc_pause_ms");
    }
    public void Begin()
    {
        if (_writer is null) return;
        _allocated = GC.GetTotalAllocatedBytes(false);
        _cpu = CpuExecutionSample.Read();
        _start = Stopwatch.GetTimestamp();
    }
    public long Mark() => _writer is null ? 0 : Stopwatch.GetTimestamp();
    public void End(ulong frame, long authorityDone)
    {
        if (_writer is null) return;
        var done = Stopwatch.GetTimestamp();
        var bytes = GC.GetTotalAllocatedBytes(false) - _allocated;
        var cpu = CpuExecutionSample.Read();
        var scale = 1000.0 / Stopwatch.Frequency;
        _writer.TryWrite(string.Create(CultureInfo.InvariantCulture,
            $"2,{frame},{(authorityDone-_start)*scale:F6},{(done-authorityDone)*scale:F6},{(done-_start)*scale:F6},{bytes},{cpu.CyclesSince(_cpu)},{(cpu.CyclesValid && _cpu.CyclesValid ? 1 : 0)},{cpu.GcPauseMillisecondsSince(_cpu):F6}"));
    }
    public void Dispose() => _writer?.Dispose();
}
