# Bounded native terrain sampling

The subsequent exact client cave-plane cache and its independent CPU oracle are
recorded in [terrain-streaming-cache.md](terrain-streaming-cache.md). The server
column-geometry cache below remains separate and preserves its scalar query API.

The server scalar generated-block reader previously called sample_column(x,z)
for every Y sample. That repeated the same climate/height noise throughout each
vertical repair scan. TerrainColumnCache now retains up to 256 geometry samples
per thread, with exact signed coordinate checks and deterministic replacement.
Misses execute the original basegame sampler unchanged.

The cache contains no material classification, generated block IDs or live
overrides. Every query still uses its current material rules and the original
Y-dependent cave-density function. Scalar out-of-world Y checks remain before
lookup. Both terrain_plan_column and terrain_generated_block use the cache.
The compiled generator seed/revision is unchanged; a future runtime seed or
generator-configuration change would require explicit cache-key/invalidation work.

Separately, block_store_snapshot_count now sums existing per-chunk block counts
instead of constructing and sorting a snapshot just to count it. This still walks
occupied chunks; it is not an O(1) counter. Filled snapshot ordering, preserved
air overrides, synchronous save ordering and dirty acknowledgement remain intact.

## Qualification

build/terrain-cache-native.log passes:

- 29912 scalar parity samples across all 512 Y values, signed/extreme coordinates,
  alternate material IDs/water heights, column plans and deliberate collisions.
- 2048 samples from four concurrent callers.
- The client WorldStream probe's 24480 terrain/parser/edit/delivery comparisons.
- Actual snapshot count/fill ABI cases, 32827 backpressure checks and 17101 fluid
  evaluator/scheduler/CABI checks.

The canonical native terrain validation target now actually executes on Windows
and Linux hosts; cross-compilation fails explicitly instead of claiming execution.
Only Windows x64 was run here.

Fixed-work CPU sampling benchmark, four alternating-order passes in one process:

| Traversal | Samples per pass | Uncached mean ms | Cached mean ms |
| --- | ---: | ---: | ---: |
| Vertical repair scan | 32768 | 11.457775 | 1.637575 |
| Fluid neighborhood | 20736 | 7.864500 | 4.040800 |
| Deliberately colliding misses | 32768 | 12.156850 | 12.206550 |

All compared workloads produce equal checksums. These are scalar CPU timings,
not client FPS. The miss-heavy case intentionally shows the cache does not help
every traversal.

The actual basegame/generated-terrain managed fixture in
logs/server/terrain-cache-probe.log passes fluid application, publication and
save/reload again. Over 120 measured authority ticks after 30 warmup ticks:
fluid-step mean/p95 is 0.688/1.175 ms; whole authority mean/p95/max is
1.015/4.036/5.000 ms. Nine changes, maximum seven pending positions, maximum 4180
reads and zero fluid budget stops. The prior fixture measured fluid mean 1.931 ms
and whole mean 2.253 ms with 81 budget stops. The same scenario runs with a time
budget, so the faster version can complete more repair work; it is not a fixed
instruction-count comparison. Use the fixed-work benchmark above for that.

This does not establish vegetation parity. The current native/client generator
lacks the active decoration stage; see vegetation-recovery.md. No world revision
or saved baseline changed in this optimization.
