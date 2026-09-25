using System.Text.Json;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.Host.Remote;

internal sealed partial class RemoteTransportClient
{
    private const int MaximumPendingWrites = 128;
    private const long MaximumPendingWriteBytes = 64 * 1024 * 1024;
    private readonly Queue<(string FileName, byte[] Payload)> _pendingWrites = new();
    private readonly HashSet<ulong> _pendingBlockAcks = new();
    private long _pendingWriteBytes;

    private void SyncFiles()
    {
        SendIntentFile(ChunkViewFile, RemoteIntentKind.ChunkView);
        SendPlayerCommands();
        SendIntentFile(BlockInteractionFile, RemoteIntentKind.BlockInteraction);
        SendIntentFile(WorldTimeFile, RemoteIntentKind.WorldTime);
        SendIntentFile(WorldItemsIntentFile, RemoteIntentKind.WorldItems);
        SendIntentFile("block_results_ack.json", RemoteIntentKind.BlockResultsAck);
    }

    private void SendIntentFile(string fileName, RemoteIntentKind kind)
    {
        var controller = _controller;
        if (controller is null)
            return;
        byte[] bytes;
        try
        {
            bytes = File.ReadAllBytes(Path.Combine(_runtimeDirectory, fileName));
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return;
        }
        if (_sent.TryGetValue(fileName, out var previous) && previous.AsSpan().SequenceEqual(bytes))
            return;
        controller.SendIntent((byte)kind, bytes);
        _sent[fileName] = bytes;
        TraceIntent(kind, bytes);
    }

    private static bool TryReadFrameIndex(string payload, out ulong frameIndex)
    {
        frameIndex = 0;
        using var document = JsonDocument.Parse(payload);
        return document.RootElement.TryGetProperty("frameIndex", out var value) &&
            value.TryGetUInt64(out frameIndex) && frameIndex != 0;
    }

    private void QueueMailbox(string fileName, byte[] payload)
    {
        if (_pendingWrites.Count >= MaximumPendingWrites ||
            _pendingWriteBytes + payload.Length > MaximumPendingWriteBytes)
        {
            Fail("error: remote mailbox backlog exceeded its limit");
            return;
        }

        _pendingWrites.Enqueue((fileName, payload));
        _pendingWriteBytes += payload.Length;
    }

    private void OnBlockAck(ulong frameIndex)
    {
        if (frameIndex == 0 || _pendingBlockAcks.Contains(frameIndex))
            return;
        if (_pendingBlockAcks.Count >= MaximumPendingWrites)
        {
            Fail("error: remote block acknowledgement backlog exceeded its limit");
            return;
        }
        _pendingBlockAcks.Add(frameIndex);
    }

    private void FlushMailboxes()
    {
        for (var count = 0; count < 8 && _pendingWrites.TryPeek(out var pending); count++)
        {
            if (!TryWriteAtomic(pending.FileName, pending.Payload))
                break;
            _pendingWrites.Dequeue();
            _pendingWriteBytes -= pending.Payload.Length;
        }

        if (_pendingBlockAcks.Count == 0)
            return;
        var path = Path.Combine(_runtimeDirectory, BlockInteractionFile);
        try
        {
            var payload = File.ReadAllText(path);
            if (!TryReadFrameIndex(payload, out var pending))
                return;
            if (_pendingBlockAcks.Contains(pending))
                File.Delete(path);
            _pendingBlockAcks.RemoveWhere(frame => frame <= pending);
        }
        catch (FileNotFoundException)
        {
            _pendingBlockAcks.Clear();
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or JsonException)
        {
            // Retry after the native reader/writer releases the mailbox.
        }
    }

    private void PublishPose()
    {
        var entity = _entity;
        if (entity is null || !entity.TryReadPose(out var pose) || pose.SourceTick == _publishedPoseTick)
            return;

        var payload = JsonSerializer.SerializeToUtf8Bytes(new
        {
            version = 1,
            source = "server_player_state_stream",
            frameIndex = pose.FrameIndex,
            acknowledgedInputFrame = pose.AcknowledgedInputFrame,
            sourceTick = pose.SourceTick,
            sourceSeconds = pose.SourceSeconds,
            worldTimeDayFraction = pose.WorldDayFraction,
            worldTimeTotalSeconds = pose.WorldTotalSeconds,
            playerX = pose.X,
            playerY = pose.Y,
            playerZ = pose.Z,
            playerPitch = pose.Pitch,
            playerYaw = pose.Yaw,
            playerVelocityX = pose.VelocityX,
            playerVelocityY = pose.VelocityY,
            playerVelocityZ = pose.VelocityZ,
            playerControlMode = pose.Flying ? 1u : 0u,
 playerOnGround = pose.OnGround ? 1u : 0u,
 jumpHeld = pose.JumpHeld ? 1u : 0u,
 simulationTick = pose.SourceTick,
 simulationTime = pose.SourceSeconds,
        });
        if (TryWriteAtomic(PlayerStateFile, payload))
        {
            _publishedPoseTick = pose.SourceTick;
            TracePose(in pose);
        }
    }

    private bool TryWriteAtomic(string fileName, byte[] payload)
    {
        var path = Path.Combine(_runtimeDirectory, fileName);
        var temporary = path + ".tmp";
        try
        {
            using (var stream = new FileStream(temporary, FileMode.Create, FileAccess.Write, FileShare.None))
                stream.Write(payload);
            File.Move(temporary, path, overwrite: true);
            return true;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return false;
        }
    }
}
