# Exact cave-noise reuse during column streaming

2026-09-13. This change addresses CPU terrain reconstruction in the existing
WorldStream worker. It does not change the seed, generator revision, voxel
resolution, material rules, server authority, scheduling or rendering backend.

## Measured bottleneck

The final radius-32 DX12 presentation run before this change is
`logs/client/validation/presentation-performance-native-final/presentation-dx12-off-required-59r_2op9`.
Its CPU records show one column at 1.437 s, 4,200 at 87.980 s, all 4,225 at
88.983 s and no pending meshes at 90.989 s. Arrival stays around 48–50 columns
per second despite hundreds of rendering frames per second early in loading.

The startup stream snapshot already contains all 4,225 column identities and
no edits; it is 101,520 bytes (120-byte header plus 24 bytes per column). The
server is not publishing these identities at the observed arrival rate.
Historical direct generator measurements around 20–21 ms per column agree with
the observed single-worker throughput. These historical CPU timings are
supporting evidence, not same-binary stage instrumentation.

GPU records contain 4,225 initial meshes and 4,221 neighbor repairs, with zero
stale discards. Initial delivery waits total roughly 27.8 s, overlapping terrain
generation. Their approximately 6.6 ms mean per arrival remains a separate frame
hitch concern; optimizing the generator does not remove synchronous GPU waits.

## Change

Three cave-noise channels previously recalculated both interpolated X/Z lattice
planes for every buried voxel. X/Z stay fixed for a vertical line, and the Y
lattice changes only every roughly 30 samples. `Noise3Column` now retains exactly
two planes per channel, keyed by full signed lattice Y. Neighboring planes occupy
different slots, including negative coordinates. Skipped, repeated and reversed
queries recompute on exact-key misses.

`CaveColumnSampler` owns a column sample and three local caches. Scalar and cached
paths use the same density/material expressions and interpolation order. The
server's random-access scalar API remains available and uses no shared mutable
cave cache. The client creates one sampler per X/Z line and applies authoritative
overrides after reconstruction, then compacts the same lossless block payload.
There are no extra worker threads or unbounded cache entries.

## Independent CPU qualification

`tools/Source/TerrainCacheProbe/Reference.h` retains the pre-change scalar
revision-2 expressions, independently of the new cache and factored helpers.
The probe checks bit-identical noise/density at lattice boundaries, every Y
level, signed coordinates up to 32 million blocks, and random/reversed/repeated
Y access. Eight complete columns compare all 4,194,304 voxels, metadata,
compacted size, edited air and a 65,535-valued override against that reference.

The isolated strict-FP build and the probe linked against the actual canonical
release `octaryn_client_world_stream.lib` both passed 258,827 checks plus the
full-column equality comparisons. The latter uses the production C++23/O2/Ob2
compiler options. Logs:

- `build/terrain-cache-oracle.log`
- `build/terrain-cache-canonical-oracle.log`

The canonical-library CPU benchmark alternates scalar/cached ordering across
five rounds and eight columns, 40 generations per path. Hashing happens outside
each timed interval; both paths include allocation, generation, edits and
compaction and produce matching column hashes.

| Path | Mean generation ms/column |
| --- | ---: |
| Original scalar reference | 20.0865 |
| Cached production library | 7.63595 |

This bounded CPU comparison measured 2.63x throughput; it is not an FPS or full
streaming speedup claim. `build/terrain-cache-canonical-benchmark.log` retains the
result. An earlier strict-FP standalone build measured 15.9072 versus 7.73232 ms;
do not mix the two compiler configurations in one comparison.

## Packaged integration

The canonical `octaryn_validate_terrain_cache`, `octaryn_validate_client_world_stream`,
`octaryn_validate_server_terrain_generation_native_probe` and
`octaryn_validate_client_player_model` targets passed without reported compiler
warnings. These retain 24,480 server/client/parser/edit/delivery comparisons,
29,912 scalar server samples, 2,048 concurrent server samples, and compact-storage
sharing/COW/residency lifecycle checks. See `build/terrain-cache-canonical-validation.log`.

The rebuilt real client/server was measured at 1440p, radius 32, FSR Off and
batched submission on both APIs. Source and package remained frozen during these
runs. The following intervals reflect the approximately one-second CSV sampling
resolution; they are not exact event timestamps.

| API | Before: all columns s | After: all columns s | Before: meshes settled s | After: meshes settled s |
| --- | ---: | ---: | ---: | ---: |
| DX12 | 87.980–88.983 | 44.728–45.729 | 89.986–90.989 | 46.731–47.735 |
| Vulkan | 87.593–88.596 | 49.570–50.576 | 89.600–90.603 | 52.579–53.580 |

Every run retains all 4,225 columns, 15,751,869 quads and 763,570,684 GPU bytes;
the stationary view draws the same 6,178,621 quads. No LOD or feature reduction.
The after runs measure approximately 222 FPS DX12 and 188 FPS Vulkan once fully
settled. This optimization accelerates population, rather than claiming a
settled-frame rendering gain.

Final evidence under `logs/client/validation/terrain-cache-final`:

- `presentation-dx12-off-required-0fpsxd15`
- `presentation-vulkan-off-required-krbrsoj7`
- `capture/rml-leqsltp1/hud`: 600 resident Vulkan frames with native validation,
  zero reported warnings/errors and an inspected 1280x720 GPU capture.

Client SHA-256: `74c956078f75e364007f87a9d020da456fda8588c7fe8e620faa25966519d7d0`.
`logs/client/terrain-cache-packaged-comparison.json` records both build snapshots,
sampling bounds and source hashes, and verifies matching world/settings/camera/
geometry/memory for each API. The after package also includes the coordinated
[FSR settings](fsr-player-settings.md) work; these are not otherwise byte-identical
builds. The isolated CPU comparison above establishes the cache benefit directly.

The final active render-closure audit still passes Slang/standalone-RHI-only
ownership (103 sources, 102 headers, 39 reachable shaders). Packaged shaders
match source. All touched code stays below 500 lines.

This pass does not qualify Linux/macOS, high-speed moving-center streaming or
hitch-free loading. Synchronous initial GPU mesh waits are the next measured
streaming bottleneck; increasing unbounded queues or hiding detail would not
address them.

Reproduce correctness with the four canonical targets above. The registered
`octaryn_terrain_cache_probe` executable accepts `--benchmark` for the alternating
scalar/cached CPU comparison. Use `benchmark_presentation.py` as documented in
[presentation-performance.md](presentation-performance.md) for packaged results.
