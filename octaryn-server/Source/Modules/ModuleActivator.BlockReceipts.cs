using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.Modules;

internal enum BlockInteractionAdmission { Deferred, Ready, Rejected, AlreadyHandled }

internal sealed partial class ModuleActivator
{
    internal const long BlockMovementWaitMilliseconds = 2000;
    private BlockReceiptLedger? _blockReceipts;
    private ulong _waitingBlockCommand;
    private long _waitingBlockSince;

    internal string? BlockResultsPath => _blockReceipts?.ResultsPath;
    internal string? BlockReceiptSession => _blockReceipts?.Session;

    // Call once per connection, before reading its first block intent.
    internal string BeginBlockReceiptSession(string runtimeDirectory)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        SaveBlockAuthority();
        _clientBlockCommands.DiscardPending();
        _waitingBlockCommand = 0;
        _blockReceipts = new(runtimeDirectory, _blockEdits.GetBlock, () => BlockRevision);
        SaveBlockAuthority();
        return _blockReceipts.Session;
    }

    internal void EndBlockReceiptSession()
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        SaveBlockAuthority();
        _clientBlockCommands.DiscardPending();
        _waitingBlockCommand = 0;
        _blockReceipts = null;
    }

    // nowMilliseconds and consumedMovementFrame are authoritative bridge state.
    // A timed-out dependency is explicitly rejected; never validate against an old pose.
    internal BlockInteractionAdmission AdmitBlockInteraction(in HostCommand command,
        ulong movementFrameID, ulong consumedMovementFrame, long nowMilliseconds)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        var receipts = _blockReceipts ?? throw new InvalidOperationException("Block receipt session was not started.");
        receipts.ReadAcknowledgement();
        if (receipts.IsKnownCommand(command.RequestId)) return BlockInteractionAdmission.AlreadyHandled;
        if (command.RequestId == 0) return BlockInteractionAdmission.Rejected;
        bool timedOut = false;
        if (movementFrameID > consumedMovementFrame)
        {
            if (_waitingBlockCommand != command.RequestId)
            {
                _waitingBlockCommand = command.RequestId;
                _waitingBlockSince = nowMilliseconds;
            }
            timedOut = nowMilliseconds >= _waitingBlockSince &&
                nowMilliseconds - _waitingBlockSince >= BlockMovementWaitMilliseconds;
            if (!timedOut) return BlockInteractionAdmission.Deferred;
        }
        if (!receipts.TryReserve(command.RequestId, new(command.A, command.B, command.C)))
            return BlockInteractionAdmission.Deferred;
        _waitingBlockCommand = 0;
        if (!timedOut) return BlockInteractionAdmission.Ready;
        receipts.Reject(command);
        return BlockInteractionAdmission.Rejected;
    }

    internal void RejectAdmittedBlockInteraction(in HostCommand command) => _blockReceipts?.Reject(command);

    private void ObserveBlockResult(HostCommand command, BlockEditResult result) => _blockReceipts?.Record(command, result);

    private void ObserveHostBlockResult(HostCommand command, BlockEditResult result)
    {
        if ((command.Flags & HostCommand.ClientInteractionFlag) != 0) ObserveBlockResult(command, result);
    }

    private void SaveBlockAuthority()
    {
        if (_blockReceipts is null) _blockPersistence.SaveIfDirty(_blocks);
        else
        {
            _blockReceipts.ReadAcknowledgement();
            _blockReceipts.SaveAndPublish(() => _blockPersistence.SaveIfDirty(_blocks), BlockRevision);
        }
    }
}
