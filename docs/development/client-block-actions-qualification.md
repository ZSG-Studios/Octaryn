# Native block-action transport qualification

Current result: **PASS** for real local transport against isolated staged current
server binaries, seven receipts and `feedback_failures=0`. Canonical bundles must
be refreshed together for binary snapshot v3 / RemoteProtocol 5 before final
packaged local/remote qualification.

Target: `octaryn_client_block_actions_probe`.
Source: `tools/Source/ClientBlockActionsProbe/main.cpp` (tools owner).
Uses production LocalSession, SessionIo, BlockReceipts, WorldStream and catalog;
no injected OS input or graphical client automation. Both local and remote
transports use the same checks. Remote runs require a separately launched server
with an isolated fresh authoritative world; the fresh-world argument is then
the client's fresh local session/cache directory.

Build only this target:

```powershell
cmake --build build/release-windows/cmake --target octaryn_client_block_actions_probe --parallel 4
```

Run with the canonical bundle's native bridge on PATH and a new world path:

```powershell
$env:PATH = "$PWD/build/release-windows/client/bundle;$env:PATH"
build/release-windows/tools/native/bin/octaryn_client_block_actions_probe.exe `
  "$PWD/build/release-windows/client/bundle" `
  "$PWD/build/release-windows/tools/validation/client-block-actions-local-fresh" `
  "$PWD/logs/tools/client-block-actions-local"
```

Append `127.0.0.1:PORT` for the remote transport. Never reuse a probe world/cache
path. Startup is bounded to 90 seconds, receipts/submission to 30 seconds, and
authoritative-column delivery to 60 seconds. The probe logs command IDs,
serialized movement dependencies when captured, consumed-input acknowledgements,
receipt sequence/revision, acceptance and authoritative values.

Checks: loaded spawn/terrain/catalog; immediate WorldStream targeting overlays;
real accepted break/place; out-of-reach rejection at unchanged world revision;
exact rollback to the original delivered block; rapid same-column edits;
serialized movement dependency beyond the acknowledgement at submission;
receipt acknowledgement and server retirement; restored original nearby blocks
after clearing all predictions. Accepted-feedback failures are collected so the
probe still restores originals and completes the other checks, then exits failed.

## Live local finding and repair, 2026-09-17

Focused native build passed. The local roundtrip exercised all seven commands:
six accepted and one rejected, with exact unchanged-revision rollback, subsequent
ack retirement, and original block restoration. It failed the immediate-feedback
check on acceptance. Evidence: `logs/tools/client-block-actions-local.log`.

`StreamColumn::revision` is a CONTENT HASH, not the server BlockRevision counter.
Example: pre-edit base hash `4953160058118402688` was compared against accepted
receipt revision `1`, prematurely removing the overlay while the base was still
the original grass block. Both WorldStream coverage and WorldPredictedBlocks
coverage currently make this invalid cross-domain comparison.

The repair adds real authoritative BlockRevision to `OCSTRM01` version 3 and
keeps content hash identity intact. Native coverage uses only the authoritative
watermark. Unchanged-content publication reuses column storage and publishes
metadata without global generation or GPU remeshing. The strict feedback check
is retained.

Post-fix isolated staging result (`logs/tools/block-authority-transport.log`):

```text
client_block_actions_checks receipts=7 movement_gated=7 exact_rollback=1 unchanged_revision=1 ack_retired=1 originals_restored=1 feedback_failures=0
client_block_actions PASS transport=local receipts=7 movement_gated=7 valid_break_place exact_rollback unchanged_revision rapid_edits baseline_cover ack_retirement originals_restored
```

Staging path is recorded in `logs/tools/block-authority-stage-path.txt`. Only the
server payload and catalog were staged under `build/release-windows/tools/validation`;
current isolated managed assemblies and current native libraries replaced the
copied server payload there. Canonical bundle directories were not modified.

Focused regression `octaryn_block_prediction_qualification` passes a huge stale
content hash vs server revision 1, later covering authority, unchanged-content
watermark advances with identical storage identity, zero-edit full baselines and
unresolved-command retention. Actual server publication qualification passes
native JSON/binary revision capture, unedited distant full baseline, and existing
durability/publication behavior. Logs: `logs/tools/block-authority-baseline.log`,
`logs/tools/block-authority-publication.log`. Renderer translation units compiled
successfully; actual GPU screenshot qualification remains separate.

The main session owns the next packaged local/remote rerun and GPU screenshot.
This task built no canonical bundle and stopped only its own LocalSession server.
