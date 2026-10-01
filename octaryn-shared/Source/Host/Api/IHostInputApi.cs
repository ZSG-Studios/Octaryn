namespace Octaryn.Shared.Host.Api;

public interface IHostInputApi
{
    // False when no input has arrived yet this session.
    bool TryPollInput(out HostInputState input);
}
