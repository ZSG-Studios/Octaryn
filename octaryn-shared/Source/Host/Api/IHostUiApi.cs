namespace Octaryn.Shared.Host.Api;

public interface IHostUiApi
{
    bool ShowNotification(string text);

    // False when no module-visible UI action is pending.
    bool TryPollAction(out string actionId);
}
