using System.Diagnostics;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

internal sealed partial class RemoteTransportClient
{
    private sealed record CommandBatch(PlayerCommand[] Commands, long Submitted);
    private sealed record PoseUpdate(SessionPose Pose);
    private CommandBatch? _commandBatch;
    private PoseUpdate? _latestPose;

    // A batch is the complete unacknowledged command suffix. Replacing it never
    // drops an unacknowledged edge; prediction owns retransmission history.
    public bool SubmitCommands(PlayerCommand[] commands)
    {
        if (commands.Length is < 1 or > PlayerCommandPacket.MaxBatch) return false;
        ulong previous = 0;
        foreach (var command in commands)
        {
            if (!PlayerCommandPacket.Valid(command) || (previous != 0 && command.FrameIndex != previous + 1))
                return false;
            previous = command.FrameIndex;
        }
        Volatile.Write(ref _commandBatch, new CommandBatch(commands, Stopwatch.GetTimestamp()));
        return true;
    }

    private void PublishTypedPose(SessionPose pose) =>
        Volatile.Write(ref _latestPose, new PoseUpdate(pose));

    public bool TryPollPose(out SessionPose pose)
    {
        var update = Interlocked.Exchange(ref _latestPose, null);
        pose = update?.Pose ?? default;
        return update is not null;
    }
}
