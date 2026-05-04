using Octaryn.Shared.Host;
using Octaryn.Shared.World;
using Octaryn.Server.World.Blocks;

namespace Octaryn.Server.Simulation.Players;

internal sealed class ServerPlayerCollision(ServerBlockStore blocks, IBlockAuthorityRules blockRules)
{
    public const float SpawnEyeHeight = 2.72f;

    private const float CollisionStep = 0.1f;
    private const float CollisionRadius = 0.3f;
    private const float CollisionHeight = 1.8f;
    private const float EyeOffset = 1.62f;
    private const float GroundContactProbe = 0.025f;
    private const float PhysicsEpsilon = 0.001f;
    private const float AirAcceleration = 6.0f;
    private const float Gravity = 24.0f;
    private const float JumpSpeed = 8.5f;

    public bool TryAlignSpawnToSurface(
        ServerPlayerState state,
        bool loadedFromSave,
        out ServerPlayerState alignedState,
        out int surfaceY,
        out BlockId surfaceBlock)
    {
        alignedState = state;
        surfaceY = 0;
        surfaceBlock = BlockId.Air;
        var columnX = FloorToInt(state.X);
        var columnZ = FloorToInt(state.Z);
        if (!TryFindSurface(columnX, columnZ, out surfaceY, out surfaceBlock))
        {
            return false;
        }

        var desiredEyeY = surfaceY + SpawnEyeHeight;
        var shouldAdjust = !loadedFromSave ||
            state.Y < desiredEyeY ||
            state.Y > desiredEyeY + 24.0f;
        if (!shouldAdjust)
        {
            return true;
        }

        alignedState = state with
        {
            Y = desiredEyeY,
            VelocityX = 0.0f,
            VelocityY = 0.0f,
            VelocityZ = 0.0f,
            IsOnGround = false
        };
        return true;
    }

    public ServerPlayerState MoveWalk(
        ServerPlayerState state,
        HostInputSnapshot input,
        float dt,
        float pitch,
        float yaw,
        float walkSpeed,
        float sprintWalkSpeed)
    {
        var speed = input.Sprint ? sprintWalkSpeed : walkSpeed;
        var target = MoveYawRelative(input.MoveX * speed, input.MoveZ * speed, yaw);
        var velocityX = state.VelocityX;
        var velocityY = state.VelocityY;
        var velocityZ = state.VelocityZ;
        if (state.IsOnGround)
        {
            velocityX = target.X;
            velocityZ = target.Z;
        }
        else
        {
            var blend = Math.Min(1.0f, AirAcceleration * dt);
            velocityX += (target.X - velocityX) * blend;
            velocityZ += (target.Z - velocityZ) * blend;
        }

        var jumpRequested = input.Jump && state.IsOnGround;
        var isOnGround = state.IsOnGround;
        if (jumpRequested)
        {
            velocityY = JumpSpeed;
            isOnGround = false;
        }
        else if (isOnGround && velocityY < 0.0f)
        {
            velocityY = 0.0f;
        }

        var position = (state.X, state.Y, state.Z);
        var hitX = MoveAxis(ref position, 0, velocityX * dt);
        var hitZ = MoveAxis(ref position, 2, velocityZ * dt);
        if (hitX)
        {
            velocityX = 0.0f;
        }

        if (hitZ)
        {
            velocityZ = 0.0f;
        }

        if (!jumpRequested && isOnGround && HasGroundContact(position))
        {
            velocityY = 0.0f;
            return state with
            {
                X = position.X,
                Y = position.Y,
                Z = position.Z,
                Pitch = pitch,
                Yaw = yaw,
                VelocityX = velocityX,
                VelocityY = velocityY,
                VelocityZ = velocityZ,
                IsOnGround = true,
                ControlMode = ServerPlayerControlMode.Walk
            };
        }

        isOnGround = false;
        velocityY -= Gravity * dt;
        var hitY = MoveAxis(ref position, 1, velocityY * dt);
        if (hitY)
        {
            if (velocityY < 0.0f)
            {
                isOnGround = true;
            }

            velocityY = 0.0f;
        }

        return state with
        {
            X = position.X,
            Y = position.Y,
            Z = position.Z,
            Pitch = pitch,
            Yaw = yaw,
            VelocityX = velocityX,
            VelocityY = velocityY,
            VelocityZ = velocityZ,
            IsOnGround = isOnGround,
            ControlMode = ServerPlayerControlMode.Walk
        };
    }

    private bool TryFindSurface(int x, int z, out int surfaceY, out BlockId surfaceBlock)
    {
        for (var y = ServerBlockLimits.WorldMaxYExclusive - 1; y >= ServerBlockLimits.WorldMinY; y--)
        {
            var block = blocks.GetBlock(new BlockPosition(x, y, z));
            if (blockRules.IsSolidBlock(block))
            {
                surfaceY = y;
                surfaceBlock = block;
                return true;
            }
        }

        surfaceY = 0;
        surfaceBlock = BlockId.Air;
        return false;
    }

    private bool MoveAxis(ref (float X, float Y, float Z) position, int axis, float delta)
    {
        if (MathF.Abs(delta) <= float.Epsilon)
        {
            return false;
        }

        var steps = Math.Max(1, (int)MathF.Ceiling(MathF.Abs(delta) / CollisionStep));
        var step = delta / steps;
        for (var index = 0; index < steps; index++)
        {
            var candidate = position;
            candidate = axis switch
            {
                0 => (candidate.X + step, candidate.Y, candidate.Z),
                1 => (candidate.X, candidate.Y + step, candidate.Z),
                _ => (candidate.X, candidate.Y, candidate.Z + step)
            };
            if (IsColliding(candidate))
            {
                Bisect(ref position, axis, step);
                return true;
            }

            position = candidate;
        }

        return false;
    }

    private void Bisect(ref (float X, float Y, float Z) position, int axis, float step)
    {
        var start = position;
        var lower = 0.0f;
        var upper = 1.0f;
        for (var index = 0; index < 8; index++)
        {
            var t = (lower + upper) * 0.5f;
            var candidate = axis switch
            {
                0 => (start.X + step * t, start.Y, start.Z),
                1 => (start.X, start.Y + step * t, start.Z),
                _ => (start.X, start.Y, start.Z + step * t)
            };
            if (IsColliding(candidate))
            {
                upper = t;
            }
            else
            {
                lower = t;
            }
        }

        position = axis switch
        {
            0 => (start.X + step * lower, start.Y, start.Z),
            1 => (start.X, start.Y + step * lower, start.Z),
            _ => (start.X, start.Y, start.Z + step * lower)
        };
    }

    private bool HasGroundContact((float X, float Y, float Z) position)
    {
        return IsColliding((position.X, position.Y - GroundContactProbe, position.Z));
    }

    private bool IsColliding((float X, float Y, float Z) position)
    {
        var minX = FloorToInt(position.X - CollisionRadius + PhysicsEpsilon);
        var minY = FloorToInt(position.Y - EyeOffset + PhysicsEpsilon);
        var minZ = FloorToInt(position.Z - CollisionRadius + PhysicsEpsilon);
        var maxX = FloorToInt(position.X + CollisionRadius - PhysicsEpsilon);
        var maxY = FloorToInt(position.Y + CollisionHeight - EyeOffset - PhysicsEpsilon);
        var maxZ = FloorToInt(position.Z + CollisionRadius - PhysicsEpsilon);
        for (var z = minZ; z <= maxZ; z++)
        for (var y = minY; y <= maxY; y++)
        for (var x = minX; x <= maxX; x++)
        {
            if (blockRules.IsSolidBlock(blocks.GetBlock(new BlockPosition(x, y, z))))
            {
                return true;
            }
        }

        return false;
    }

    private static (float X, float Z) MoveYawRelative(float x, float z, float yaw)
    {
        var yawSine = MathF.Sin(yaw);
        var yawCosine = MathF.Cos(yaw);
        return (
            yawCosine * x + yawSine * z,
            -(yawCosine * z) + yawSine * x);
    }

    private static int FloorToInt(float value)
    {
        return (int)MathF.Floor(value);
    }
}
