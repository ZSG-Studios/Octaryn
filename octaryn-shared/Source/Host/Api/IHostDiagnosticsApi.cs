namespace Octaryn.Shared.Host.Api;

public interface IHostDiagnosticsApi
{
    void Write(HostLogLevel level, string message);
}
