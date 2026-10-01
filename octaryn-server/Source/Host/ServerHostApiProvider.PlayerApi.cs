using Octaryn.Server.Simulation.Players;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Host;

internal sealed partial class ServerHostApiProvider
{
    private sealed class PlayerApi : IHostPlayerApi
    {
        private readonly PlayerController _player;

        public PlayerApi(PlayerController player)
        {
            _player = player;
        }

        public bool TryGetState(out HostCharacterState state)
        {
            var snapshot = _player.Snapshot();
            state = new HostCharacterState
            {
                X = snapshot.X,
                Y = snapshot.Y,
                Z = snapshot.Z,
                Pitch = snapshot.Pitch,
                Yaw = snapshot.Yaw,
                VelocityX = snapshot.VelocityX,
                VelocityY = snapshot.VelocityY,
                VelocityZ = snapshot.VelocityZ,
                IsOnGround = snapshot.IsOnGround,
                ControlMode = snapshot.ControlMode,
                JumpHeld = snapshot.JumpHeld,
            };
            return true;
        }

        public void SetState(in HostCharacterState state)
        {
            _player.SetState(new PlayerState(
                state.X,
                state.Y,
                state.Z,
                state.Pitch,
                state.Yaw,
                state.VelocityX,
                state.VelocityY,
                state.VelocityZ,
                state.IsOnGround,
                state.ControlMode,
                state.JumpHeld));
        }
    }
}
