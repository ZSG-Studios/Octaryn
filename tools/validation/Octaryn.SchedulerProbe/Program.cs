using ClientScheduler = Octaryn.Client.Host.HostScheduler;
using ServerScheduler = Octaryn.Server.Tick.HostScheduler;

return SchedulerProbe.Run();

internal static partial class SchedulerProbe
{
    public static int Run()
    {
        ValidateInvalidWorkerCounts("client", count => new ClientScheduler(count));
        ValidateInvalidWorkerCounts("server", count => new ServerScheduler(count));
        ValidateDefaultCapacity("client", () => new ClientScheduler());
        ValidateDefaultCapacity("server", () => new ServerScheduler());

        using var client = new ClientScheduler(2, ProbeDeclarations("client", 2));
        using var server = new ServerScheduler(2, ProbeDeclarations("server", 2));

        ValidateScheduler("client", client, () => client.Diagnostics);
        ValidateScheduler("server", server, () => server.Diagnostics);
        ValidateTopology("client", client.Diagnostics, client.WorkerThreadCapacity);
        ValidateTopology("server", server.Diagnostics, server.WorkerThreadCapacity);
        ValidateShutdownUnresolvedWorkDiagnostics(
            "client",
            () => new ClientScheduler(2, UnresolvedProbeDeclarations("client")),
            scheduler => scheduler.Diagnostics);
        ValidateShutdownUnresolvedWorkDiagnostics(
            "server",
            () => new ServerScheduler(2, UnresolvedProbeDeclarations("server")),
            scheduler => scheduler.Diagnostics);
        ValidateDisposedScheduler("client", new ClientScheduler(2));
        ValidateDisposedScheduler("server", new ServerScheduler(2));
        return 0;
    }
}
