namespace Octaryn.Shared.Host;

internal interface IHostCommandSink
{
    // Host implementations must define thread-safety and backpressure before worker-thread use.
    bool Enqueue(HostCommand command);
}
