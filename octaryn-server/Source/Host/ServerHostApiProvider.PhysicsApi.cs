using Octaryn.Server.Simulation.Players;
using Octaryn.Server.World.MapWorld;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Host;

internal sealed partial class ServerHostApiProvider
{
    private sealed class PhysicsApi : IHostPhysicsApi
    {
        private readonly IntPtr _mapWorld;

        public PhysicsApi(IntPtr mapWorld)
        {
            _mapWorld = mapWorld;
        }

        public bool Raycast(
            float originX, float originY, float originZ,
            float directionX, float directionY, float directionZ,
            float maxDistance, out HostRaycastHit hit)
        {
            return NativeMapWorld.Raycast(
                _mapWorld,
                originX, originY, originZ,
                directionX, directionY, directionZ,
                maxDistance, out hit) == 0;
        }

        public unsafe bool MoveCharacter(in HostCharacterInput input, double deltaSeconds, ref HostCharacterState state)
        {
            var nativeInput = new NativeInput(
                input.Flags,
                input.Controller,
                input.MoveX,
                input.MoveY,
                input.MoveZ,
                input.CameraX,
                input.CameraY,
                input.CameraZ,
                input.CameraPitch,
                input.CameraYaw,
                input.RelativeMouse);
            var nativeState = new NativeState(
                state.X, state.Y, state.Z,
                state.Pitch, state.Yaw,
                state.VelocityX, state.VelocityY, state.VelocityZ,
                state.IsOnGround ? 1u : 0u,
                state.ControlMode,
                (ushort)(state.JumpHeld ? 1 : 0));
            var tickResult = default(NativeTickResult);

            var result = NativeMapWorld.StepInto(
                _mapWorld, &nativeInput, deltaSeconds, &nativeState, &tickResult);
            if (result != 0)
            {
                return false;
            }

            state.X = nativeState.X;
            state.Y = nativeState.Y;
            state.Z = nativeState.Z;
            state.Pitch = nativeState.Pitch;
            state.Yaw = nativeState.Yaw;
            state.VelocityX = nativeState.VelocityX;
            state.VelocityY = nativeState.VelocityY;
            state.VelocityZ = nativeState.VelocityZ;
            state.IsOnGround = nativeState.IsOnGround != 0;
            state.ControlMode = nativeState.ControlMode;
            state.JumpHeld = nativeState.JumpHeld != 0;
            return true;
        }

        public bool StepWorldItem(ref HostWorldItemState state, double deltaSeconds)
        {
            var nativeState = new HostWorldItemStateNative
            {
                X = state.X,
                Y = state.Y,
                Z = state.Z,
                VelocityX = state.VelocityX,
                VelocityY = state.VelocityY,
                VelocityZ = state.VelocityZ,
                Grounded = state.Grounded ? 1u : 0u,
                Sleeping = state.Sleeping ? 1u : 0u,
                SleepTimer = state.SleepTimer
            };

            if (NativeMapWorld.StepItem(_mapWorld, ref nativeState, deltaSeconds) != 0)
            {
                return false;
            }

            state.X = nativeState.X;
            state.Y = nativeState.Y;
            state.Z = nativeState.Z;
            state.VelocityX = nativeState.VelocityX;
            state.VelocityY = nativeState.VelocityY;
            state.VelocityZ = nativeState.VelocityZ;
            state.Grounded = nativeState.Grounded != 0;
            state.Sleeping = nativeState.Sleeping != 0;
            state.SleepTimer = nativeState.SleepTimer;
            return true;
        }
    }
}
