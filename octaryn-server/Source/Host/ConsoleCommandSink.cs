using Octaryn.Shared.Host;

namespace Octaryn.Server.Host;

internal sealed class ConsoleCommandSink : IHostCommandSink
{
    public bool Enqueue(HostCommand command)
    {
        _ = command;
        return false;
    }
}
