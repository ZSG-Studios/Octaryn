using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Host;

internal sealed partial class ServerHostApiProvider
{
    private sealed class DiagnosticsApi : IHostDiagnosticsApi
    {
        public void Write(HostLogLevel level, string message)
        {
            LiveDebugLog.Write($"module_api level={level} {message}");
        }
    }
}
