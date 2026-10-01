using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Host;

internal sealed partial class ServerHostApiProvider
{
    private sealed class UiApi : IHostUiApi
    {
        private readonly ServerHostApiProvider _owner;

        public UiApi(ServerHostApiProvider owner)
        {
            _owner = owner;
        }

        // The authority has no notification surface; notifications travel with
        // the replication channel instead. Report unhandled, never fake-shown.
        public bool ShowNotification(string text)
        {
            return false;
        }

        public bool TryPollAction(out string actionId)
        {
            if (_owner._uiActions.Count == 0 || (_owner._replicationChannel?.AvailableChangeCapacity ?? int.MaxValue) < 2)
            {
                actionId = string.Empty;
                return false;
            }

            actionId = _owner._uiActions.Dequeue();
            return true;
        }
    }
}
