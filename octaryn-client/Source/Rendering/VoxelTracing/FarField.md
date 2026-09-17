# Far-field CPU representation

This is a bounded CPU foundation for the future voxel trace publisher, not an
integrated GPU far-world tracer or a completed SRC cutover. No raster mesh LOD is
introduced. All code is independently authored.

## Publisher contract

- `FarFieldKey` stores signed global **cell** indices, not block origins or
  rebased floats. Levels 0/1/2 have block widths 4/16/64. Use `far_field_key` for
  floor division at negative coordinates. Generation validates the int32 block
  domain before calling the existing terrain functions.
- `FarFieldNode` contains 64 children in `x + 4*(y + 4*z)` order. `known` means
  that child's entire contents are covered. `occupied` means there is at least
  one non-air material witness. These are independent: a partially known parent
  can contain a known witness without being complete. Only `complete()` with
  zero occupied bits proves empty space.
- `uniform` means a complete child has one material ID throughout, not that it
  is an opaque cube. The 64 material IDs preserve exact leaf material identity;
  nonuniform parent IDs must not be used for shading. Host-supplied material
  feature flags are unioned without discarding cutout, fluid, transparent or
  emissive categories. Shape intersection still belongs to the shared tracer.
- Masks are host uint64 values. The GPU publisher should pack these into two
  uint32 words each, explicitly; this C++ struct is not an asserted shader ABI.
- `far_field_resident()` reads a 4³ leaf directly from the immutable/composed
  `StreamColumn`, preserving its authoritative revision and all block IDs.
- `far_field_aggregate()` builds a 16/64 node from 64 smaller nodes. Pass default
  unknown nodes for missing children. An occupied macro is a traversal candidate,
  never an exact full-macro hit or an invented height-field occluder.
- `FarFieldCache` bounds retained nodes (default 4096) with FIFO eviction.
  Invalidate a changed voxel or whole replaced column before publishing its new
  content; these calls erase affected 4/16/64 ancestors and advance the ticket.
  A publication started before any invalidation is rejected. Reset with a new
  world epoch when world/generator identity changes. Publisher jobs and upload
  queues must also be bounded by their existing scheduler owners.

## Generator and authoritative coverage

`far_field_generate()` returns `Ready`, `Refine`, `Deferred` or `Unknown`.
Its caller supplies per-call column-sample, voxel-sample and edit-record budgets.
It calls the actual revision-3 `TerrainColumn`, `TerrainDensity` and
`TerrainVegetation` functions used by `GenerateColumn.cpp`, including caves,
water, protected roofs, halo trees/bushes/flowers and ordered authoritative edits.
The current generator functions themselves do not consume the snapshot seed;
this adapter does not invent a separate seed policy.

Generation uses at most `(width+2)^2` terrain-column samples for conservative
height envelopes, including the vegetation halo, rather than `width^3` voxel
queries for 16/64 macros. The upper envelope includes water and the six-block
maximum vegetation rise. Known generated air can be summarized directly with
sparse edits. Intersecting macros return `Refine`; exact cave/material evaluation
is restricted to requested 4³ leaves (64 samples). No sampled-height bound is
misrepresented as exact underground occupancy. This can still require many
bounded refinements in complicated cave regions; no full-horizon cost claim is
made.

**Required input not provided by today's resident stream:** a complete,
revisioned authoritative-edit snapshot for each requested far key, including
proof when that edit set is empty. `FarFieldAuthority::complete` is precisely
that guarantee, and its key must match. Missing resident columns do not establish
it. Without it, cells inside the valid vertical world remain unknown. Generator
revision 3 proves air outside Y=[-256,256), where valid edits are prohibited;
unsupported generators or X/Z domains remain unknown. Far horizon exhaustion
must remain unknown rather than unoccluded sky until these coverage guarantees
and the traversal policy are integrated.

## Verification and references

`tools/validation/far_field_test.cpp` compares generated leaves to actual full
`generate_stream_column` outputs, covering terrain, sand/water, trees, leaves,
bushes and flowers. It additionally checks signed/domain boundaries, absent
authority, ordered edited air, material flags, work exhaustion, unknown child
aggregation, cache capacity, ancestor invalidation, column invalidation and stale
publication rejection. The isolated Windows CPU executable passes 178 checks;
logs: `logs/build/far-field-build.log`, `logs/client/far-field-cpu.log`.
No GPU or platform-runtime qualification is implied.

NanoVDB's [HDDA reference](https://github.com/AcademySoftwareFoundation/openvdb/blob/master/nanovdb/nanovdb/math/HDDA.h)
was inspected for variable-size aligned cells and hierarchy traversal contracts.
Its implementation was not copied. Octaryn's explicit unknown/authority masks
address a world-stream coverage distinction absent from a fully supplied VDB.
