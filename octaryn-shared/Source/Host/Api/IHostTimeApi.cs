namespace Octaryn.Shared.Host.Api;

public interface IHostTimeApi
{
    double NowSeconds { get; }

    ulong TickId { get; }

    double TickRate { get; }
}
