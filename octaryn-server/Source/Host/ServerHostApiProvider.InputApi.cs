using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Host;

internal sealed partial class ServerHostApiProvider
{
    private sealed class InputApi : IHostInputApi
    {
        private readonly ServerHostApiProvider _owner;

        public InputApi(ServerHostApiProvider owner)
        {
            _owner = owner;
        }

        public bool TryPollInput(out HostInputState input)
        {
            if (_owner._latestInput is not { } snapshot)
            {
                input = default;
                return false;
            }

            // Mirror the native has_input_intent contract: a neutral snapshot is
            // telemetry, not a look — the character keeps its current angles.
            if (snapshot is { Controller: 0, Flags: 0, MoveX: 0.0f, MoveY: 0.0f, MoveZ: 0.0f, RelativeMouse: 0 })
            {
                input = default;
                return false;
            }

            input = new HostInputState(
                snapshot.Flags,
                snapshot.Controller,
                snapshot.MoveX,
                snapshot.MoveY,
                snapshot.MoveZ,
                snapshot.CameraPitch,
                snapshot.CameraYaw);
            return true;
        }
    }
}
