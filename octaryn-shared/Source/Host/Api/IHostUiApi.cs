namespace Octaryn.Shared.Host.Api;

public interface IHostUiApi
{
    bool ShowNotification(string text);

    // False when no module-visible UI action is pending.
    bool TryPollAction(out string actionId);

    // Text fields are bounded by the declared screen; no markup or file paths are accepted.
    bool TryPresentScreen(string assetId, IReadOnlyDictionary<string, string> fields) => false;
    bool HideScreen(string assetId) => false;
}
