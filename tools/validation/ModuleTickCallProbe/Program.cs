using Octaryn.Server.Modules;
using Octaryn.Shared.Host;

void Check(bool value, string message) { if (!value) throw new Exception(message); }
using var runtime = new NativeScheduleRuntime();
var thread = Environment.CurrentManagedThreadId;
var calls = 0;
var fail = false;
var sentinel = new InvalidOperationException("Expected callback failure");
using var callback = new ModuleTickCall(runtime, () =>
{
    Check(Environment.CurrentManagedThreadId == thread, "Owner-thread affinity changed");
    Check(NativeJobsLibrary.IsCommandWriteScopeActive, "Command-write scope missing");
    ++calls;
    if (fail) throw sentinel;
});
for (var i = 0; i < 10000; ++i) callback.Execute();
Check(calls == 10000 && !NativeJobsLibrary.IsCommandWriteScopeActive, "Call count/scope restoration failed");
var allocated = GC.GetAllocatedBytesForCurrentThread();
for (var i = 0; i < 10000; ++i) callback.Execute();
Check(GC.GetAllocatedBytesForCurrentThread() == allocated, "Repeated successful calls allocated");
fail = true;
try { callback.Execute(); throw new Exception("Callback exception concealed"); }
catch (InvalidOperationException error) { Check(ReferenceEquals(error, sentinel), "Exception identity changed"); }
Check(!NativeJobsLibrary.IsCommandWriteScopeActive, "Failed callback leaked scope");
fail = false;
callback.Execute();
Check(calls == 20002, "Exception state leaked into subsequent call");
ModuleTickCall? nested = null;
using (nested = new ModuleTickCall(runtime, () => nested!.Execute()))
{
    try { nested.Execute(); throw new Exception("Reentrant callback accepted"); }
    catch (InvalidOperationException error) { Check(error.Message.Contains("already executing"), "Unexpected reentrant error"); }
}
callback.Dispose();
try { callback.Execute(); throw new Exception("Disposed callback executed"); }
catch (ObjectDisposedException) { }
Check(!NativeJobsLibrary.IsCommandWriteScopeActive, "Reentrancy leaked command scope");
Console.WriteLine("module_tick_call_probe=passed repeated=20000 allocated_bytes=0 exception_identity=1 recover_after_failure=1 owner_thread=1 scope_restored=1 disposed_rejected=1 reentrant_rejected=1");
