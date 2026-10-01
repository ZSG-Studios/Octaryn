using System.Runtime.InteropServices;

namespace Octaryn.Server;

// Cycles measure executing owner-thread work; never convert them to milliseconds.
internal readonly record struct CpuExecutionSample(ulong Cycles, long GcPauseTicks, bool CyclesValid)
{
    public static CpuExecutionSample Read()
    {
        ulong cycles = 0;
        var valid = OperatingSystem.IsWindows() && ThreadCycles.Read(ThreadCycles.Current(), out cycles);
        return new(cycles, GC.GetTotalPauseDuration().Ticks, valid);
    }

    public ulong CyclesSince(CpuExecutionSample start) =>
        CyclesValid && start.CyclesValid && Cycles >= start.Cycles ? Cycles - start.Cycles : 0;
    public double GcPauseMillisecondsSince(CpuExecutionSample start) =>
        Math.Max(0, GcPauseTicks - start.GcPauseTicks) / (double)TimeSpan.TicksPerMillisecond;
}

internal static partial class ThreadCycles
{
    [LibraryImport("kernel32.dll", EntryPoint = "GetCurrentThread")]
    internal static partial IntPtr Current();

    [LibraryImport("kernel32.dll", EntryPoint = "QueryThreadCycleTime")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static partial bool Read(IntPtr thread, out ulong cycles);
}
