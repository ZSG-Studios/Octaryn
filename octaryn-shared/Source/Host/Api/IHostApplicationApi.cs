namespace Octaryn.Shared.Host.Api;

// Current client session only. Accepted requests end the normal host loop;
// they do not terminate a process or bypass host cleanup.
public interface IHostApplicationApi
{
    bool RequestExit();
}
