# Voxel throughput: bounded work and frame overlap

Status: 2026-09-13. This slice preserves exact voxel geometry, material passes,
server authority and the no-LOD requirement. A separate user-authorized task is
migrating the backend to Direct3D 12 through standalone Slang RHI. Source backend
selection is changing concurrently; the Vulkan results below qualify the
preserved Vulkan package, not the evolving DX12 package.

## Coalesce loading-related halo refreshes

`WorldMeshHalo.cpp` retains ordinary dirty coordinates until all eight neighbors
inside the current render window have source data. Coordinates outside that
window are intentionally absent. Readiness uses the current window on each
selection, so shrink and center reversal cannot leave a cached wait condition.
Urgent edits, neighbor unloads and stale retries bypass this loading delay.
Completion polling still runs when no new dirty coordinate is eligible.

The original reference's `world/jobs/taskflow/scheduler.cpp` checks all nine
block states before starting a mesh job. The active implementation adopts that
readiness rule for halo refreshes while preserving immediate initial meshes.
`WorldStream::poll` still publishes the exact query payload immediately before
synchronous initial meshing; camera and targeting run afterward. This slice
does not make initial delivery asynchronous or expose unrendered query data.

The prior `logs/client/stream-final-after-gpu.csv` records 13,228 jobs:
4,225 initial columns and 9,003 halo attempts, including 215 discarded results.
For static inputs and an unchanged complete window, coalescing permits at most
one ordinary halo refresh per column. That is a work-reduction opportunity,
not a measured frame-rate improvement. If expected source data never arrives,
ordinary work deliberately remains pending under the complete-window contract.

## Two graphics frames with separately owned mutable resources

`WorldFrames.h` tracks two reusable slots with monotonically increasing fence
values. A slot must complete before its resources are overwritten. `WorldTargets`
separates depth, G-buffer, HDR scene and final color textures; `WorldBatch` gives
each slot its own mapped records, indirect arguments and retained mesh handles.
Immutable assets are shared. Uploads and rendering remain ordered on one queue.

`WorldGpuProfile` pairs each timestamp pool with its frame's CPU metadata and
resolves it after the corresponding fence. Final draining preserves frame order,
including an odd final frame. Capture waits for the selected output frame;
resize drains outstanding frames before replacing targets. The explicit
one-frame mode remains available for comparison.

This follows the resource-reuse principle in Khronos's
[wait-idle versus fences sample](https://docs.vulkan.org/samples/latest/samples/performance/wait_idle/README.html).
Its performance numbers are external examples, not Octaryn results. Initial
column count/readback/emit still uses synchronous waits; two graphics frames do
not remove that remaining streaming dependency.

## Rejected experiment: cooperative emission

The experimental `WorldGreedy.slang` kept the row masks and lexicographic
rectangle partition. Its group leader collected up to 32 rectangles, reserved
contiguous face and patch ranges with two global atomic additions, and all
group threads wrote the batch. Group barriers avoided assuming a particular
hardware subgroup width. Face records, patch topology, material eligibility
and transparent ordering remained exact.

[Binary Greedy Meshing](https://github.com/cgerikj/binary-greedy-meshing)
illustrates occupancy masks, directional face masks and material-aware merging.
[NVIDIA's aggregated-atomics explanation](https://developer.nvidia.com/blog/cuda-pro-tip-optimized-filtering-warp-aggregated-atomics/)
motivates reserving a range once for cooperating threads. Octaryn uses Slang
group-shared storage and explicit barriers in the experiment; it did not depend
on CUDA. Fewer atomics did not establish a speedup.

The root's eight same-binary ABBAABBA runs measured:

| Fixture | Original mean | Cooperative mean | Change |
| --- | ---: | ---: | ---: |
| Solid | 0.121246 ms | 0.160375 ms | +32.3% |
| Checkerboard | 1.285493 ms | 0.880083 ms | -31.5% |
| Stepped | 0.135826 ms | 0.159629 ms | +17.5% |

All oracle comparisons remained exact with zero reported errors. The common
solid/stepped regressions rejected this optimization: the original shader was
restored in source and the preserved package. Qualification tests remain.
The retained improvements from this task are frame overlap and halo coalescing.

## Qualification and measurements

The GPU portion of `build/deep-voxel-validation.log` reports:

- Two slots across five frames: premature resubmission rejected, readback
  sentinels and profile association exact, odd-frame drain ordered.
- Cooperative output: 60 cases, six directions, up to 1,024 rectangles per
  plane and 32 per batch, with the original partition preserved.
- Full GPU mesh/raster parity and the existing scratch-reuse lifecycle checks.
- A complete 3x3 arrival fixture: eight halo jobs, zero early center refreshes,
  plus urgent edit/retry, unload and reversal/shrink checks.

That combined build ended unsuccessfully because the CPU draw-preparation
fixture still expected the former cached resource count. The GPU pass markers
do not turn that aggregate into a pass. The corrected CPU rerun subsequently
passed in `logs/client/dx12-draw-preparation.log`: 3,356 halo checks, 140 draw
checks and 12,006 culling checks, without creating a GPU device.

### Preserved Vulkan comparison

`work/deep-comparison.json` joins `logs/client/deep-before-1440*` against
`logs/client/deep-after-1440-vulkan*`. Both hidden runs use 2560x1440, radius 32,
the same copied player/world state, and the user's PBR/POM settings with fog off.
Recorded settled conditions match exactly: 4,225 columns, 15,221,650 quads,
1,578 drawn columns, 5,555,607 drawn quads and player eye
`(1258.058,45.620,-1340.283)`. Main/GPU report joins match 106/106 before and
101/101 after. The logs do not contain a rendered camera yaw/pitch/FOV capture;
the comparison therefore establishes matching recorded conditions, not an
independently captured identical camera projection.

| Measurement | Before | After |
| --- | ---: | ---: |
| Final benchmark mean frame time | 10.779 ms | 4.863 ms |
| Reciprocal mean, approximate FPS | 93 | 206 |
| Final benchmark 1% low FPS | 63.49 | 114.29 |
| Final benchmark worst frame | 102.693 ms | 12.635 ms |
| First full residency, startup bounds | 83.614–84.626 s | 86.933–87.936 s |
| Full residency and no pending meshes | 92.682–93.978 s | 88.938–90.434 s |
| Total mesh jobs | 13,174 | 8,446 |
| Published halo refreshes | 8,748 | 4,221 |
| Discarded halo results | 201 | 0 |
| Tracked retained GPU bytes | 621,670,488 | 769,464,488 |

Initial residency became slower, while the complete boundary backlog finished
earlier. The retained memory cost increased by 147,794,000 bytes, approximately
141 MiB, for frame overlap and retained resources. The final `world_profile`
aggregates contain 928 samples before and 2,058 after. Loading still includes
an approximately 107 ms hitch; these results do not establish hitch-free play.
Milestones are bounds between periodic reports. CPU CSV stage percentiles are
report/interval statistics, not all-frame percentiles; final benchmark aggregate
statistics above come from the explicit `world_profile` records.

The original executable SHA-256 was
`E990090B2F7A2900F2DD65870D782A6C781A9CB18CE5A44DD216C04A89F06DAC`.
The measured after executable in `work/deep-vulkan-qualified` was
`15DB458C1AC587A921A3091DBA5354C5D5276FBB593CA413DD2D3E069CB8F332`,
with the original shader restored after the rejected cooperative experiment.
The first `deep-after-1440` attempt collided with concurrent DX12 startup changes
and failed startup; it is excluded from this comparison.

The separately evolving DX12 package passes the 838-source-file staging check
in `logs/client/deep-current-stage.json`. That check does not transfer these
Vulkan timings or graphics proofs to DX12. The preserved Vulkan package also
passes synchronization validation with two actual frame slots:

- `deep-final-distance`: radius 4 -> 8 -> 4 reaches 81 -> 289 -> 81 columns,
  completes each pending queue, and exits normally through domain UI actions.
  Its GPU capture validates 268,600 rectangles covering 1,383,708 unit faces.
- `deep-final-fluid`: packaged HDR, player, animated materials and RmlUi run;
  captured water/lava cover all eight levels and 18 fixture cases. Sixty fluid
  faces match the independent oracle, with zero errors and maximum absolute
  coordinate error 5.96e-8. The profiler drains its final two-slot records.
- `build/deep-final-batch.log`: the restored shader passes actual batch slot
  ownership/reuse, replacement/empty parity and 256 oblique seam views, with
  zero uncovered interior pixels. The isolated production probe uses the same
  executable as the earlier full GPU suite and the restored original shader.

These runs report no GPU validation errors. Four known unused shader-varying
warnings remain in the packaged fluid run. No OS input events were injected.
Sustained manual traversal and other-platform performance remain unqualified.

## Deferred opportunities, still without LOD

- **Directional draw masks:** [Sodium's chunk renderer](https://github.com/CaffeineMC/sodium/blob/dev/common/src/main/java/net/caffeinemc/mods/sodium/client/render/chunk/DefaultChunkRenderer.java)
  intersects camera-facing directions with nonempty geometry slices. An Octaryn
  implementation needs direction-specific ranges and exact tests around camera
  planes, cutouts and two-sided materials; current whole-column culling is not
  that implementation.
- **Conservative occlusion:** [Niagara](https://github.com/zeux/niagara) and its
  [draw-culling shader](https://github.com/zeux/niagara/blob/master/src/shaders/drawcull.comp.glsl)
  demonstrate GPU visibility and depth-pyramid occlusion. Any adaptation must
  preserve visibility after camera motion, resize and disocclusion, and exclude
  unsuitable transparent occluders. Hi-Z is not implemented by this slice.
- **Asynchronous initial delivery:** split dequeue from visible-query commit,
  retain exact immutable source/neighbor identities, and publish query plus mesh
  only after verified GPU completion. Bound pending work and handle eviction,
  edits and reversal before removing the current synchronous contract.
- **Further allocation reduction:** emitted meshes still own independent output
  buffers. Consider bounded suballocation only after backend-specific profiling,
  with descriptor lifetime and fence retirement tests. Avoid replacing measured
  redundant work with an unqualified global allocation scheme.
