# Mesh-independent voxel trace residency

## Inspected source and ownership

The real input is `WorldPresentation/WorldStream/WorldStream.h::StreamColumn`,
not mesher quads or the older planning payloads in `Rendering/VoxelWorld`.
Columns are 32 blocks wide, normally span signed Y=-256 through 255, and store
block IDs as `x + 32 * ((world_y - min_y) + height * z)`.
`ColumnBlocks.h` publishes lossless, shared palette-packed pages and supports
bounded row reads without expansion. `PredictedColumn.h` composes reversible
presentation edits over the authoritative base; accepted predictions retire only
once the base's durable authoritative revision covers their receipt.

`Rendering/VoxelTracing` consumes that exact composed column representation.
It does not generate another terrain approximation, mesh, or acceleration
structure. Block zero is air; every other material is an occupancy candidate.
The shared GPU material catalog and shape intersection must decide opacity,
cutouts, fluids and shaped surfaces after traversal reaches that candidate.

## CPU owner and bounds

`voxel_tracing::VoxelTraceWorld::publish(column)` builds immutable 32-cubed
chunk snapshots from one bounded column. Each chunk has:

- 512 4-cubed leaf masks, one uint64 per leaf;
- eight 16-cubed macro masks, one bit per nonempty child leaf;
- an eight-bit chunk macro-occupancy mask;
- an integer chunk key, geometry epoch and valid local-Y interval;
- a shared reference to the original lossless column for exact material reads.

The default CPU limits are 8,192 chunks, 4,096 retained change notifications and
1,024 blocks of height per publication. Callers select an explicit near-field
residency window and process a bounded number of publications; this owner never
walks or generates the entire render-distance radius. Capacity rejection leaves
the old state intact and does not silently evict other geometry.

Unchanged metadata does not bump geometry epochs. Occupancy-preserving material
changes do bump them, so emissive/color changes can invalidate lighting. Removed
and reloaded chunks receive new identities. Journal overflow is explicit and
requires GPU-owner resynchronization. Current vertical chunks share the current
column snapshot rather than pinning a different historical column each; upload
jobs may retain immutable old chunks until their own bounded work completes.

Missing chunks and Y outside a partial column are `Unknown`, not air. Published
all-air chunks remain known empty. CPU samples preserve exact block IDs and
epochs. No native device, render mesh, BLAS or private thread is owned here.

## Portable GPU contract

`VoxelTraceTypes.h::ChunkHeader` is 48 bytes:

| Byte | Field |
| --- | --- |
| 0 | signed int3 global chunk coordinate |
| 12 | uint macro occupancy mask |
| 16 | uint64 geometry epoch, or uint2 low/high |
| 24 | uint leaf element offset |
| 28 | uint macro element offset |
| 32 | uint packed-material word offset |
| 36 | uint inclusive minimum local Y |
| 40 | uint exclusive maximum local Y |
| 44 | uint flags; bit 0 means known chunk |

`TraceChunk::export_payload` exports a single chunk with zero base offsets;
the RHI upload owner assigns actual pool offsets. Leaf/macro masks are uint64
elements, portable as uint2 low/high in Slang. Material storage packs two uint16
block IDs into each uint32; even voxel is low 16 bits, odd is high 16 bits.
Voxel order is `x + 32*(y + 32*z)`.

Leaf index is `x/4 + 8*(y/4 + 8*(z/4))`; its voxel bit is
`x%4 + 4*(y%4 + 4*(z%4))`. Macro index is
`x/16 + 2*(y/16 + 2*(z/16))`; its child bit is
`(x/4)%4 + 4*((y/4)%4 + 4*((z/4)%4))`.

One complete exported payload uses 69,744 bytes. This is an upload contract, not
permission to allocate every CPU-resident chunk densely on the GPU without the
GPU owner's separate byte budget. Sparse lookup, pool allocation, RHI upload,
fence retirement and DDA segment status are separate owners.

`relative_chunk_origin` subtracts integer world anchors before converting to
float and refuses differences outside the exact integer float range. Global
keys never depend on the camera-relative floating-point position.

## Integration hooks

The renderer owns one `VoxelTraceWorld`. Publish the latest composed voxel
snapshot before meshing, then consume `changes_since` at the voxel upload pass.
Source locations inspected for integration:

- `WorldPredictedBlocks.cpp::compose`: after composition, before mesh-dirty work;
  applies predictions, rejection rollback and authoritative rebase.
- `WorldRenderer.cpp::open_world_renderer_update`: before `world_renderer_mesh`.
- `WorldDeliveryJobs.cpp::pump`: current mesh-completed publication happens too
  late for independent tracing. The ready `stream.peek` snapshot is available
  before `job.mesh.start`. Trace publication must use its correctly rebased
  prediction overlay, without retiring query-owner predictions prematurely.
- Renderer residency/window changes: remove trace columns independently of
  `r.columns`, because a trace-ready column may never reach mesh publication
  after a teleport/cancel. Unknown residency must remain explicit to DDA/SRC.

Do not attach trace publication only to `world_renderer_store_column`: that
would preserve the mesh dependency the new architecture is removing.

## Qualification

`tools/validation/voxel_trace_world_test.cpp` exercises actual packed column
reads, independently reconstructed leaf/macro masks, GPU ID export, partial
signed columns, immutable snapshots, material-only epochs, edited air, bounded
residency, overflow/reload, actual prediction/acknowledgement ordering and
large-coordinate integer rebasing.

Build target: `octaryn_voxel_trace_qualification`; owner library:
`octaryn_client_voxel_tracing`. Run the tool directly from
`build/release-windows/tools/native/bin/`. A standalone clang-cl qualification
also writes `logs/build/voxel-trace-cpu.log` and its executable under
`build/release-windows/tools/voxel-trace/`.

This establishes the CPU residency/occupancy foundation only. It is not a claim
of a completed SRC cutover, GPU DDA execution, far-field reconstruction,
material-shape parity, runtime frame-time improvement or platform qualification.
