using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed class ClientBlockCommandQueue(BlockCommandSink blockCommands, IBlockAuthorityRules authorityRules)
{
    public const int MaxPendingCommands = 4096;

    private readonly Queue<HostCommand> _commands = new(MaxPendingCommands);

    public int PendingCount => _commands.Count;

    public bool Enqueue(HostCommand command)
    {
        if (_commands.Count >= MaxPendingCommands || !CanQueue(command))
        {
            Octaryn.Server.LiveDebugLog.Write($"server_live_client_command_queue queued=0 pending={_commands.Count} kind={command.Kind} request={command.RequestId} edit={BlockCommandDiagnostics.EditLabel(command)} block=({command.A},{command.B},{command.C},{command.D})");
            return false;
        }

        _commands.Enqueue(command);
        Octaryn.Server.LiveDebugLog.Write($"server_live_client_command_queue queued=1 pending={_commands.Count} kind={command.Kind} request={command.RequestId} edit={BlockCommandDiagnostics.EditLabel(command)} block=({command.A},{command.B},{command.C},{command.D})");
        return true;
    }

    public int Drain()
    {
        var applied = 0;
        while (_commands.TryDequeue(out var command))
        {
            if (blockCommands.Enqueue(command))
            {
                applied++;
            }
        }

        Octaryn.Server.LiveDebugLog.Write($"server_live_client_command_drain applied={applied} pending={_commands.Count}");
        return applied;
    }

    internal bool CanQueue(HostCommand command)
    {
        return command.IsCurrent &&
            command.Kind == HostCommandKind.SetBlock &&
            command.D is >= ushort.MinValue and <= ushort.MaxValue &&
            (command.D == BlockId.Air.Value || authorityRules.IsClientPlaceable(new BlockId((ushort)command.D))) &&
            (((command.Flags & HostCommand.ClientInteractionFlag) != 0u) || blockCommands.CanEnqueue(command));
    }
}
