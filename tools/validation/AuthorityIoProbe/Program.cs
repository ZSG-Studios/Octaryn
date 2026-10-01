using Octaryn.Server;
using Octaryn.Server.Persistence.World;

void Check(bool value, string message) { if (!value) throw new Exception(message); }
var cpuStart = CpuExecutionSample.Read();
Thread.SpinWait(200000);
var cpuEnd = CpuExecutionSample.Read();
if (OperatingSystem.IsWindows()) Check(cpuStart.CyclesValid && cpuEnd.CyclesValid &&
    cpuEnd.CyclesSince(cpuStart) > 0, "Owner-thread execution cycles unavailable or regressed");
Check(cpuEnd.GcPauseMillisecondsSince(cpuStart) >= 0, "GC pause counter regressed");
NativePersistencePlayerState State(float x) => new(x, 3, -3, -.1f, 1.2f);
using var entered = new ManualResetEventSlim();
using var resume = new ManualResetEventSlim();
var writes = new List<float>();
using (var queue = new PlayerSaveQueue(state =>
{
    entered.Set();
    if (!resume.Wait(5000)) throw new TimeoutException("Test writer was not released");
    writes.Add(state.X);
}))
{
    Check(queue.TryEnqueue(State(1)), "First save was not admitted");
    Check(entered.Wait(2000), "Writer did not start");
    Check(queue.Completed is null, "Queued write falsely acknowledged as persisted");
    Check(queue.TryEnqueue(State(2)), "Second save was not admitted");
    Check(!queue.HasCapacity && !queue.TryEnqueue(State(3)), "Pending bound was exceeded");
    var flush = Task.Run(queue.Flush);
    Check(!flush.Wait(30), "Flush returned before storage completed");
    resume.Set();
    Check(flush.Wait(2000), "Flush failed to observe completion");
    Check(writes.SequenceEqual([1f, 2f]), "Save order changed");
    Check(queue.Completed is { Sequence: 2 } receipt && receipt.State.X == 2, "Latest completion was not actual written state");
    Check(queue.TryEnqueue(State(3)), "Backpressured save did not resume");
}
Check(writes.SequenceEqual([1f, 2f, 3f]), "Shutdown failed to drain admitted writes");
var failed = new PlayerSaveQueue(_ => throw new IOException("expected injected storage failure"));
Check(failed.TryEnqueue(State(4)), "Failure fixture admission");
try { failed.Flush(); throw new Exception("Storage error was concealed"); }
catch (IOException) { }
try { failed.Dispose(); throw new Exception("Shutdown concealed storage error"); }
catch (IOException) { }

using var logEntered = new ManualResetEventSlim();
using var logResume = new ManualResetEventSlim();
var lines = new List<string>();
using (var log = new BoundedLogWriter(null, false, 2, line =>
{
    logEntered.Set();
    if (!logResume.Wait(5000)) throw new TimeoutException("Test diagnostic writer was not released");
    lines.Add(line);
}))
{
    Check(log.TryWrite("first") && logEntered.Wait(2000), "Log writer did not start");
    Check(log.TryWrite("second") && log.TryWrite("third"), "Log queue capacity changed");
    Check(!log.TryWrite("overflow") && log.Dropped == 1, "Log overflow was not explicit");
    logResume.Set();
}
Check(lines.SequenceEqual(["first", "second", "third"]), "Log order/drain changed");
Console.WriteLine("authority_io_probe=passed save_bound=2 completion_after_write=1 ordered_shutdown=1 failure_visible=1 diagnostic_backpressure=1");
