namespace Octaryn.Shared.Host.Api;

internal unsafe sealed partial class NativeHostApiProvider
{
    public IHostPhysicsApi? GetPhysicsApi()
    {
        if (_query is null)
        {
            return null;
        }

        var table = (HostPhysicsApiTable*)_query(HostApiTableIds.Physics, 1u);
        if (table is null ||
            table->Version < 1u ||
            table->Size < 16u ||
            table->Raycast is null)
        {
            return null;
        }

        return new NativePhysicsApi(table);
    }

    private sealed class NativePhysicsApi : IHostPhysicsApi
    {
        private readonly HostPhysicsApiTable* _table;

        public NativePhysicsApi(HostPhysicsApiTable* table)
        {
            _table = table;
        }

        public bool Raycast(
            float originX, float originY, float originZ,
            float directionX, float directionY, float directionZ,
            float maxDistance, out HostRaycastHit hit)
        {
            var nativeHit = default(HostRaycastHitNative);
            var result = _table->Raycast(
                originX, originY, originZ,
                directionX, directionY, directionZ,
                maxDistance, &nativeHit);
            if (result != 0 || nativeHit.Hit == 0)
            {
                hit = default;
                return false;
            }

            hit = new HostRaycastHit(
                nativeHit.MaterialId,
                nativeHit.PointX, nativeHit.PointY, nativeHit.PointZ,
                nativeHit.NormalX, nativeHit.NormalY, nativeHit.NormalZ,
                nativeHit.Distance, nativeHit.TriangleIndex);
            return true;
        }

        public bool MoveCharacter(in HostCharacterInput input, double deltaSeconds, ref HostCharacterState state)
        {
            // move_character arrives with table version 2 / size 24; the
            // step_world_item slot grows the table to version 3 / size 32.
            if (_table->Size < 24 || _table->MoveCharacter is null)
            {
                return false;
            }

            var nativeInput = new HostCharacterInputNative
            {
                Flags = input.Flags,
                Controller = input.Controller,
                MoveX = input.MoveX,
                MoveY = input.MoveY,
                MoveZ = input.MoveZ,
                CameraX = input.CameraX,
                CameraY = input.CameraY,
                CameraZ = input.CameraZ,
                CameraPitch = input.CameraPitch,
                CameraYaw = input.CameraYaw,
                RelativeMouse = input.RelativeMouse
            };
            var nativeState = new HostCharacterStateNative
            {
                X = state.X,
                Y = state.Y,
                Z = state.Z,
                Pitch = state.Pitch,
                Yaw = state.Yaw,
                VelocityX = state.VelocityX,
                VelocityY = state.VelocityY,
                VelocityZ = state.VelocityZ,
                IsOnGround = state.IsOnGround ? 1u : 0u,
                ControlMode = state.ControlMode,
                JumpHeld = state.JumpHeld ? 1u : 0u
            };

            if (_table->MoveCharacter(&nativeInput, deltaSeconds, &nativeState) != 0)
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
            // step_world_item arrives with table version 3 / size 32.
            if (_table->Size < sizeof(HostPhysicsApiTable) || _table->StepWorldItem is null)
            {
                return false;
            }

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

            if (_table->StepWorldItem(&nativeState, deltaSeconds) != 0)
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
