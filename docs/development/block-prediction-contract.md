# Command-correlated block prediction

**Acceptance coverage repaired:** baseline `StreamColumn::revision` remains the
content hash. Separate `authoritative_revision` now carries server BlockRevision
through binary snapshot v3 and drives accepted-overlay retirement. The real local
transport qualifier passes all seven receipts with `feedback_failures=0` using
fresh isolated staging. Evidence and rerun command:
[native block-action qualification](client-block-actions-qualification.md).

Block command identity is nonzero `commands[0].requestId` in the existing local
intent, currently equal to its `frameIndex` and returned by LocalSession as the
out command ID. Receipts call this `commandID`. It is independent of movement
input sequencing. One intent contains one predicted edit. Never infer acceptance
from intent-file deletion.

## Mailboxes (protocol v1)

`runtime/block_results.json` is atomically replaced:
`{"version":1,"session":"<server nonce>","receipts":[{"sequence":1,"commandID":42,"accepted":true,"revision":7,"blocks":[{"x":1,"y":2,"z":3,"block":0}]}]}`.
Receipts are ordered by strictly increasing sequence in that server session.
Blocks are authoritative post-command values, including secondary removals.
Rejection has unchanged current revision and requested-cell authoritative value.
Acceptance is published only after durable save. Failed save cannot acknowledge.

`runtime/block_results_ack.json` is atomically replaced by the consumer:
`{"version":1,"session":"<same nonce>","sequence":1}`.
Ack means the receipt was applied to the prediction ledger, not merely read.
The server retains unacknowledged receipts, capacity 256; reserve before consuming
an intent and stop consuming when full. Ignore different-session, regressing and
beyond-published acks. Replay is idempotent by (session,sequence).
Remote transport forwards the complete ordered batch and monotonic ack, never a
latest-result field. Reconnect clears overlays only, never durable world edits.

## Integration ownership

Native predictor: return assigned command ID from submit_block_edit; poll batch
and deliver before acknowledging. Refuse submission at prediction capacity 256.
Main WorldSession: share command ID between renderer and WorldStream; dispatch
exact-command receipts and reset both on reconnect.
Movement protocol: add batch/ack transport. Server bridge must reserve receipt
capacity before consuming block intents; consumed does not mean accepted.

Accepted overlays persist until a delivered column revision covers the receipt.
Unrelated revisions never remove pending overlays. Rejection removes exactly its
command. Recompose authoritative base plus ordered overlays; update mesh,
neighbors and lighting. No timeout resolves an unknown command.

## Authoritative baseline version

`OCSTRM01` binary version **3** adds uint64 authoritative BlockRevision immediately
after uint64 window epoch (offset 20). Header size is now 128 bytes; column and
block records are unchanged. RemoteProtocol is **5**, rejecting older peers that
cannot decode this baseline. Remote transport forwards these bytes unchanged.
Receipt mailbox JSON remains version 1.

Publication captures ModuleActivator.BlockRevision after the completed authority
tick and durable save, then synchronously exports the same BlockStore before any
next tick. The watermark is present even when the complete baseline has zero
override records. JSON diagnostic snapshots expose `authoritativeBlockRevision`.

SnapshotColumn/StreamColumn and renderer-retained authoritative bases keep this
watermark separately from the original content hash. A metadata-only advance
reuses immutable column storage and the existing bounded two-entry delivery
mailbox. Renderer publication skips GPU meshing when authoritative content/storage
is unchanged; it recomposes/remeshes only if covered accepted overlays retire.
Unresolved commands never retire from a baseline revision alone.

## Concrete client hooks (implemented)

`WorldStream::can_predict()`, `predict_block(commandID,x,y,z,block)`,
`resolve_block(commandID,accepted,revision)`, `reset_predictions()`.
Renderer counterparts: `open_world_renderer_can_predict(renderer)`,
`open_world_renderer_apply_predicted_edit(renderer,commandID,x,y,z,block)`,
`open_world_renderer_resolve_predicted_edit(renderer,commandID,accepted,revision)`,
`open_world_renderer_reset_predictions(renderer)`.
Check both capacities before submit, then add both overlays with its assigned ID.
Dispatch each receipt to both before mailbox ack. Reset on session replacement.
The apply renderer API now requires commandID (no legacy overload).

## Movement ordering admission

Block intent metadata should include `movementFrameID`, the latest owning input
frame sent before the click. Protocol owner must defer intent admission until
the server has consumed at least that movement frame; never use client camera
position for reach. This is independent of block `requestId`/intent `frameIndex`.
An intent awaiting movement or receipt capacity stays unconsumed. On reconnect
abandon that connection's unconsumed intents instead of replaying old movement.
`BlockReceiptLedger.TryReserveAfterMovement(commandID, cell, movementFrameID,
consumedMovementFrame)` combines this guard with bounded receipt reservation.
Use the authoritative consumed frame for this connection, not a received frame.
Waiting for movement neither consumes receipt capacity nor advances command ID.

## Concrete server hooks (implemented)

### Production module integration (wired)

ModuleActivator now owns the ledger, binds both command observers, and routes
both Tick and TickHostOnly plus disposal through the real persistence barrier.
The bridge must use these signatures (no direct ledger construction):

```csharp
string ModuleActivator.BeginBlockReceiptSession(string runtimeDirectory);
void ModuleActivator.EndBlockReceiptSession();
string? ModuleActivator.BlockResultsPath { get; }
string? ModuleActivator.BlockReceiptSession { get; }
BlockInteractionAdmission ModuleActivator.AdmitBlockInteraction(
    in HostCommand command, ulong movementFrameID,
    ulong consumedMovementFrame, long nowMilliseconds);
void ModuleActivator.RejectAdmittedBlockInteraction(in HostCommand command);
```

Call Begin exactly once at local/remote connection setup, and again only for a
new connection. It saves previously applied authority, drops only unexecuted old
queue commands, installs a fresh nonce, and atomically publishes an empty batch.
Pass `Environment.TickCount64` for nowMilliseconds and the owning connection's
authoritative consumed movement frame. Admission results:

- `Ready`: receipt capacity reserved; submit this one command to the existing
  SubmitClientCommands path. Real queue/sink validation and drain produce receipt.
- `Deferred`: movement not consumed or ledger full; do not submit/delete/mark
  frame consumed. Retry the same intent next iteration.
- `Rejected`: do not submit; consume the intent. Movement dependencies waiting
  2000 ms have an explicit staged rejection, published after the next real save.
  Zero command IDs are malformed and cannot produce correlated receipts.
- `AlreadyHandled`: duplicate command already reserved/completed; consume the
  duplicate without resubmission. Results remain in the ordered mailbox.

After `Ready`, bridge-level validation failure must call
RejectAdmittedBlockInteraction; definitive queue validation failures are observed
automatically. Queue capacity `-1` is retryable and retains its reservation without
staging a rejection; keep and retry the same admitted command batch.
Keep ticking after rejection so the save/publication path runs at unchanged
revision. No additional bridge save/receipt calls are needed. End persists edits
and drops unexecuted old-connection commands; it never clears the world.

The lower-level APIs below describe implementation details, not remaining wiring:

`BlockReceiptLedger(runtimeDirectory, blockEdits.GetBlock, () => BlockRevision)`
is connection-scoped and server-thread owned. At connection replacement create a
new ledger/nonce, keeping the world and persistence objects. Block command IDs
must increase within that connection (reservation retains a high-water mark).
Module construction binds the queue observer; the host sink observer additionally
requires ClientInteractionFlag so unrelated module request IDs cannot resolve
client reservations. Both hooks run after changed-edits callbacks increment
BlockRevision; submit-time validation failures
also produce real rejected results. All-or-nothing native queue batch validation
is preserved. Do not record success merely from SubmitClientCommands's count.

Module admission calls `ledger.ReadAcknowledgement()` then
`ledger.TryReserve(command.RequestId, new BlockPosition(command.A,command.B,command.C))`
BEFORE queue submission or deleting its intent. Refuse/defer if false. The local
intent has exactly one command; do not reserve a partial multi-command batch.
After host tick/drain, call
`ledger.SaveAndPublish(() => blockPersistence.SaveIfDirty(blocks), BlockRevision)`.
Save executes before any result file replacement; exceptions retain staged
receipts and capacity. This wraps all existing module SaveIfDirty sites.
Idle iterations must still read acks and publish after save so rejected commands
at unchanged revision are delivered. `ledger.Reject(command)` is available for
bridge-level validation failures after reservation (malformed intents without
an authenticated command identity cannot create a receipt).

The protocol owner must wire the module admission API in ChunkStreamProcessBridge,
which this owner does not edit. Module hooks are already wired. Result mailbox copies
belong in the same connection runtime directory as block intent files.

## Owner qualification (2026-09-17)

Passed native `tools/Source/BlockPredictionQualification/main.cpp` against the
production prediction ledger and column compositor: unchanged-revision rejection,
rapid same-cell overlays, unrelated authoritative revision, receipt-before-base
and base-before-receipt, capacity 256, reconnect, immutable base and restored
boundary/mesh-source storage identity. This tests mesh inputs, not GPU output.

Passed `tools/validation/BlockReceiptQualification` using production receipt
ledger/contracts: ordered batches, actual authoritative cell values, failed-save
barrier, wrong-session/future/regressing ack rejection, replay prevention after
ack, bounded reservations, new-session mailbox replacement and world preservation.
Run with:
`dotnet run --project tools/validation/BlockReceiptQualification/BlockReceiptQualification.csproj -p:OctarynBuildPresetName=release-windows -- logs/tools/block-receipt-qualification`.

Server/shared build passed with zero warnings/errors under isolated
the isolated managed qualification tree (now preserved under
`build/release-windows/tools/validation/block-prediction-qualification`). WorldRenderer,
WorldPredictedBlocks, WorldDeliveryJobs and WorldStream translation units compiled
with the existing release-windows compilation flags into
`build/release-windows/tools/block-prediction/native/bin`. Qualification logs are
`logs/tools/block-prediction-qualification.log` and
`logs/tools/block-receipt-qualification.log`.

Full session wiring and actual GPU/runtime qualification remain with the main
and protocol owners. No canonical bundle was rebuilt or user process terminated.
The documented reference checkout and REQUESTS.md were absent in this checkout;
actual BlockCommandSink, ClientBlockCommandQueue, native queue atomic validation,
BlockEditService and persistence failure propagation were inspected directly.

## Production authority/receipt CLI qualification

The module hooks are now integrated, not left to the bridge owner. The dedicated
`--block-receipts` path in Octaryn.ServerWorldBlocksProbe runs actual
ModuleActivator, native BlockCommandQueue validation/drain, actual world saves,
and serialized receipt/ack mailboxes. It uses an isolated generated world and
player directory under
`build/release-windows/tools/validation/block-prediction-qualification/worlds`.

Run before or after packaging, pointing at the desired production native DLLs:

```powershell
tools/validation/qualify_block_receipts.ps1 `
  -NativeDirectory build/release-windows/server/native/bin `
  -SharedNativeDirectory build/release-windows/shared/native/bin
```

For packaged qualification, pass the bundled server directory to NativeDirectory
and the directory containing octaryn_native_jobs.dll to SharedNativeDirectory
(if omitted, the native server directory is used). The script only builds the
isolated managed qualification owners; it never rebuilds/packages canonical
bundles. `-NoBuild` reuses that already-built qualification assembly.

Validated: real client-interaction placement and break using authoritative player
pose; out-of-reach rejection at unchanged revision; movement dependency defer and
explicit timeout rejection; rapid same-column commands with distinct receipts;
on-disk readback before module disposal; actual native-save failure under a file
lock with no premature accepted result; retry publication; 256-result admission
backpressure; ack-driven mailbox retirement; reconnect nonce and pending-queue
reset; durable authoritative value after a new module reopens the world.
The native command queue is also deliberately filled: transient capacity returns
no receipt, and retry of the same admitted command later produces its correlated
acceptance. Definitive validation failure and transient pressure are distinct.

This CLI covers production server authority through serialized result/ack files.
LocalSession remote/local transport dispatch, WorldStream overlay dispatch and
GPU screenshots remain separate integration qualification by their owners.
Latest production result: PASS, with zero build warnings/errors. Full output:
`logs/tools/block-receipt-production-qualification.log`.
