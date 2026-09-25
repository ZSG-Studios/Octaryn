namespace Octaryn.Shared.Host;

public interface IHostCommandSink
{
    // Host implementations must define thread-safety and backpressure before worker-thread use.
    bool Enqueue(HostCommand command);
}
