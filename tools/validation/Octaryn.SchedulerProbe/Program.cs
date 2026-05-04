using Octaryn.Client.ClientHost;
using Octaryn.Server.Tick;

return SchedulerProbe.Run();

internal static partial class SchedulerProbe
{
    public static int Run()
    {
        ValidateInvalidWorkerCounts("client", count => new ClientHostScheduler(count));
        ValidateInvalidWorkerCounts("server", count => new ServerHostScheduler(count));
        ValidateDefaultCapacity("client", () => new ClientHostScheduler());
        ValidateDefaultCapacity("server", () => new ServerHostScheduler());

        using var client = new ClientHostScheduler(2, ProbeDeclarations("client", 2));
        using var server = new ServerHostScheduler(2, ProbeDeclarations("server", 2));

        ValidateScheduler("client", client, () => client.Diagnostics);
        ValidateScheduler("server", server, () => server.Diagnostics);
        ValidateTopology("client", client.Diagnostics, client.WorkerThreadCapacity);
        ValidateTopology("server", server.Diagnostics, server.WorkerThreadCapacity);
        ValidateShutdownUnresolvedWorkDiagnostics(
            "client",
            () => new ClientHostScheduler(2, UnresolvedProbeDeclarations("client")),
            scheduler => scheduler.Diagnostics);
        ValidateShutdownUnresolvedWorkDiagnostics(
            "server",
            () => new ServerHostScheduler(2, UnresolvedProbeDeclarations("server")),
            scheduler => scheduler.Diagnostics);
        ValidateDisposedScheduler("client", new ClientHostScheduler(2));
        ValidateDisposedScheduler("server", new ServerHostScheduler(2));
        return 0;
    }
}
