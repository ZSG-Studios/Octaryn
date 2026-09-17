# Far-field authoritative edit coverage — proposal

Inspection only, 2026-09-17. No wire/version or runtime implementation change is
included. Read alongside `networking-recovery.md` and
`octaryn-client/Source/Rendering/VoxelTracing/FarField.md`.

## Main decision — 2026-09-17

Approved direction, scheduled after the near-field trace/upload path is GPU
qualified: implement the bounded `QueryEditCoverage` service beside the server
`BlockStore`, with separate session mailboxes and a separate coverage journal
hooked at native `BlockStore::set_block` / `clear_block_override`. The existing
chunk-view window, `StreamSnapshot`, and `GenerateColumn` remain unchanged.
Remote protocol v5 and connection key `octaryn-remote-v4` stay unchanged until
the coordinated transport extension is implemented and qualified. Near-field
tracing must not wait for this service; missing far coverage remains explicitly
unknown, never sky.

## Existing path and reusable authority

1. Server `Persistence/WorldBlocks/WorldOverrides/WorldBlockPersistence.cs::Load`
   loads persisted overrides into server `World/Blocks/Store/BlockStore`.
2. Native `BlockStore` stores overrides in sparse 32³ section maps. Presence is
   distinct from block ID zero: `try_get_block` distinguishes edited air from
   absent override. `BlockEditService.cpp::apply_override` removes an override
   when an edit restores procedural content.
3. `BlockStore::snapshot_chunk_column` probes 16 vertical section maps, sorts and
   returns overrides only. It never generates terrain. `ChunkColumnRequest.cpp`
   and `ChunkColumnStream.cpp` use this for the requested near window.
4. `ChunkStreamProcessPublication.cs::PublishSnapshot` runs after the existing
   authority tick and writes a full replacement override snapshot. Its effective
   metadata-only flag is deliberately false; omitted near overrides are not a
   delta. `ChunkStreamBinarySnapshot.cpp` writes `OCSTRM01`, version 3, including
   intent epoch, authoritative block revision, generator identity and an explicit
   record for each column, including zero-edit columns.
5. Client `WorldPresentation/WorldStream/StreamSnapshot.cpp` validates those
   records. `GenerateColumn.cpp` generates the actual voxel columns locally.
   `WorldStream.cpp` bounds generation/delivery to the resident request.
6. Remote `RemoteSession.cs::PublishSnapshot` sends the same binary bytes through
   `SessionEntity.SendSnapshot`; client `RemoteTransportMailboxes.cs` writes the
   native runtime mailbox. Player commands/pose have separate owners.

Thus the server already has edit-only authority beyond rendering residency.
The missing component is an independently bounded **coverage query/publication**,
not server generation of far chunks or client access to server save files.

Do not enlarge the existing chunk-view window for this: it also controls near
generation/residency and the server fluid simulation region. Do not send far
records through `StreamSnapshot`/`GenerateColumn`.

## Minimal server query

Add an owner-local native region-query implementation beside `BlockStore`,
exposed through `NativeBlockStoreLibrary.cs` and an owner service reached through
`ModuleActivator`. Proposed method:

`QueryEditCoverage(globalCellKey, level, recordCapacity, workBudget)`

Levels 0/1/2 mean aligned 4/16/64-block cubes. Query only stored overrides:

- A 64³ key covers at most eight 32³ sections. Each stored entry in those aligned
  sections lies in the key; stop at the record cap and return `Refine` rather
  than allocating/copying the remaining records.
- A 4³/16³ key lies within one section. Bounded point lookups (64/4096 positions)
  avoid scanning all 32768 entries in a densely edited section for a tiny query.
- A complete 16³ edit set has at most 4096 records. Proposed per-response cap:
  4096 records / 64 KiB. Dense 64³ queries request smaller keys instead.
- `Complete` with zero records is a positive proof of no overrides in this key.
  Budget exhaustion, unsupported generation and partial work are not complete.
- Preserve zero-valued override records. Sort complete results deterministically;
  complete replacement semantics also represent overrides removed from the store.

Capture data and revision on the existing server-owned authority/scheduler
barrier. Publish after `SaveBlockAuthority` has successfully saved the captured
changes, following the existing receipt durability ordering. Do not run another
tick or move native mutable maps to unsynchronized worker reads.

## Proposed separate contract

Names are provisional, with numbers/transport version requiring approval.
Request:

```
FarCoverageRequest v1
  serverSession, worldEpoch, interestEpoch
  generatorId, generatorRevision, generatorMode, seed, catalogIdentity
  desiredKeys[]: {x:i32, y:i32, z:i32, level:0|1|2}
```

The desired set replaces the previous bounded far interest. Proposed starting
limits: 1024 watched keys, 32 newly processed keys per scheduler slice, a separate
byte/work budget, and no more than two 64-KiB response packets per service step.
These are implementation starting limits, not a qualified horizon/performance
claim. Unserved coverage remains unknown. Superseded interest should be coalesced
rather than appended to an unbounded queue.

Response per key (full replacement, not delta):

```
FarCoverageResult v1
  serverSession, worldEpoch, interestEpoch, responseSequence
  generatorIdentity, key
  authoritativeRevision, sourceMutationSerial
  status: Complete | Refine | Deferred | Unsupported
  editCount
  edits[]: {x:i32, y:i32, z:i32, block:u16}
```

For `Complete`, all edits must be inside the exact key and supported vertical
world bounds. Zero records are valid and meaningful. Do not truncate a result
and retain `Complete`. Unsupported/unknown generator bounds remain unknown.

## Change tracking and stale rejection

The existing global `ModuleActivator.BlockRevision` is useful as a snapshot
watermark, but alone does not identify changed far keys. `OnBlocksChanged` has
coordinates for ordinary edits; the fluid path currently calls
`MarkBlockPersistenceDirty` with a count only. Hooking only ordinary edits misses
fluids. Do not drain/steal the existing gameplay `BlockChangeQueue`.

Preferred complete hook: native `BlockStore::set_block` and
`clear_block_override`, after an actual change, advance a store mutation serial
and append bounded changed coordinates/sections to a separate coverage journal.
Load/world replacement resets its world epoch. This catches override deletion,
edited air, normal edits and fluid writes at their common storage owner.

At the post-save publication point, send invalidations for watched keys intersected
by journal changes, with a monotonic invalidation sequence and mutation serial.
Journal overflow must explicitly invalidate all subscribed coverage, never drop
changes silently. A snapshot's source serial is captured atomically with its edit
records. The client retains the latest required serial per affected key.

Client acceptance requires matching session/world/generator/catalog identity,
current interest epoch, a requested key, valid complete counts/bounds and a source
serial no older than that key's latest invalidation or accepted result. Identical
duplicates are harmless; stale/out-of-order responses cannot restore known bits.
Reconnect, world switch and invalidation-sequence gaps clear coverage. A response
with the same serial but different content is invalid.

Replacing a key's complete edit set invalidates all overlapping far descendants
and ancestors, cancels pre-change build tickets, and emits a geometry-epoch/AABB
change for later SRC invalidation. Add a bounded `FarFieldCache::invalidate_region`
hook for macro invalidations; the existing voxel/column hooks cover finer changes.
Child responses may be aggregated only while their current dependency tickets
remain valid. Predictions remain reversible client overlays over authority, not
edits written into this server coverage cache.

## Local/remote integration map

- Server new owner: `World/Chunks/FarCoverage/` service and binary writer;
  `World/Blocks/Store/` bounded sparse-region query/journal.
- `ChunkStreamProcessBridge.HandleSessionPaths`: invoke coverage service after
  existing tick/save/pose work under its own budget. Keep near publication and
  `SetFluidRegion` unchanged.
- Separate runtime mailboxes: proposed `far_coverage_request.json` and
  `far_coverage.bin`. These are session transport artifacts, not save-file reads.
- Client new `WorldPresentation/FarCoverage/` parser/cache feeds immutable
  `FarFieldAuthority` views directly to the trace publisher; no full-column
  generation/delivery and no change to `WorldStream` residency.
- Local session I/O owner reads/writes those mailboxes with bounded/coalesced
  buffers independently of movement and durable receipt queues.
- Remote intent addition: `RemoteProtocol.RemoteIntentKind`, server
  `RemoteSessionIntents.OnIntent/FlushIntents`, client
  `RemoteTransportMailboxes.SyncFiles`.
- Remote response addition: append matching RPC registration to **both**
  `SessionEntity.cs` copies; wire client receive and server post-save publication.
  Keep coverage backlog separate from the existing 128-entry/64-MiB mailbox queue
  so it cannot displace receipts or create movement head-of-line work.

Current `RemoteProtocol.Version` is 5, connection key is `octaryn-remote-v4`, and
the near binary snapshot remains version 3. All are unchanged by this proposal.
Approval must include coordinated capability/version handling for the additional
intent/RPC; existing LES registration order and player SyncVars must not move.

## Required validation after approval

Test complete-empty versus absent/deferred, edited air, restoring generated
content, dense 64³ refinement, negative/domain edges, revisions changed during
query, stale/duplicate/reordered responses, invalidation overflow, reconnect,
prediction rollback, fluid edits and bounded starvation. Compare local and remote
coverage byte/semantic identity and verify unchanged movement/receipt behavior
under the existing impairment qualification. No protocol or runtime test has
been performed for this inspection-only proposal.
