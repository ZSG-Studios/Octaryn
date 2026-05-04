using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class ServerBlockCommandSink(
    ServerBlockEditService blockEdits,
    ServerBlockChangeQueue? blockChanges = null,
    Action<IReadOnlyList<BlockEdit>>? changedEdits = null,
    IHostCommandSink? fallback = null) : IHostCommandSink
{
    private const float ClientInteractionReach = 6.0f;
    private const float ClientInteractionReachSquared = ClientInteractionReach * ClientInteractionReach;

    public bool Enqueue(HostCommand command)
    {
        if (!CanEnqueue(command))
        {
            Octaryn.Server.LiveDebugLog.Write($"server_live_block_command rejected=1 kind={command.Kind} request={command.RequestId} edit={ServerBlockCommandDiagnostics.EditLabel(command)} block=({command.A},{command.B},{command.C},{command.D})");
            return false;
        }

        return command.Kind switch
        {
            HostCommandKind.SetBlock => ApplySetBlock(command),
            _ => fallback?.Enqueue(command) ?? false
        };
    }

    public bool CanEnqueue(HostCommand command)
    {
        if (!command.IsCurrent)
        {
            return false;
        }

        return command.Kind switch
        {
            HostCommandKind.SetBlock => CanApplySetBlock(command),
            _ => fallback is not null
        };
    }

    private bool CanApplySetBlock(HostCommand command)
    {
        return command.D is >= ushort.MinValue and <= ushort.MaxValue &&
            CanApplyClientInteraction(command) &&
            blockEdits.CanApply(new BlockEdit(
                new BlockPosition(command.A, command.B, command.C),
                new BlockId((ushort)command.D)));
    }

    private bool ApplySetBlock(HostCommand command)
    {
        var result = blockEdits.Apply(new BlockEdit(
            new BlockPosition(command.A, command.B, command.C),
            new BlockId((ushort)command.D)));
        Octaryn.Server.LiveDebugLog.Write($"server_live_block_command rejected=0 kind={command.Kind} request={command.RequestId} edit={ServerBlockCommandDiagnostics.EditLabel(command)} applied={(result.Applied ? 1 : 0)} changed={(result.Changed ? 1 : 0)} block=({command.A},{command.B},{command.C},{command.D})");
        if (result.Changed)
        {
            foreach (var change in result.Changes)
            {
                blockChanges?.Enqueue(change);
            }

            changedEdits?.Invoke(result.Changes);
        }

        return result.Applied;
    }

    private bool CanApplyClientInteraction(HostCommand command)
    {
        if ((command.Flags & HostCommand.ClientInteractionFlag) == 0u)
        {
            return true;
        }

        if (!float.IsFinite(command.X) ||
            !float.IsFinite(command.Y) ||
            !float.IsFinite(command.Z) ||
            !float.IsFinite(command.X2) ||
            !float.IsFinite(command.Y2) ||
            !float.IsFinite(command.Z2))
        {
            return false;
        }

        var editPosition = new BlockPosition(command.A, command.B, command.C);
        var hitPosition = new BlockPosition(
            (int)MathF.Round(command.X2),
            (int)MathF.Round(command.Y2),
            (int)MathF.Round(command.Z2));
        var hitBlock = blockEdits.GetBlock(hitPosition);
        if (hitBlock == BlockId.Air)
        {
            return false;
        }

        var hitCenterX = hitPosition.X + 0.5f;
        var hitCenterY = hitPosition.Y + 0.5f;
        var hitCenterZ = hitPosition.Z + 0.5f;
        var deltaX = command.X - hitCenterX;
        var deltaY = command.Y - hitCenterY;
        var deltaZ = command.Z - hitCenterZ;
        var reachSquared = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
        if (reachSquared > ClientInteractionReachSquared)
        {
            return false;
        }

        if (command.D == BlockId.Air.Value)
        {
            return editPosition == hitPosition;
        }

        return ManhattanDistance(editPosition, hitPosition) == 1 &&
            blockEdits.GetBlock(editPosition) == BlockId.Air;
    }

    private static int ManhattanDistance(BlockPosition left, BlockPosition right)
    {
        return Math.Abs(left.X - right.X) +
            Math.Abs(left.Y - right.Y) +
            Math.Abs(left.Z - right.Z);
    }
}
