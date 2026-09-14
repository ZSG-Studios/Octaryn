# Voxel streaming latency repair

The September 13 interactive session at 2560x1440 and radius 32 recorded 20-29 ms
frames with hundreds of pending boundary refreshes. The earlier settled 720p
batching benchmark did not qualify this loading workload.

## Changes

- `WorldMeshJob` stages block, metadata and argument uploads in the count
  command buffer. An explicit 160-byte readback and SlangRHI fence replace the
  synchronous device readback submission. Exact counts still determine output
  allocation; the production Slang mesher and surface topology are unchanged.
- `WorldHaloJobs` owns two jobs and starts at most one per frame. Count, emit
  and publication advance by polling completion fences. Previous meshes remain
  drawable. Publication requires the same retained primary buffer and all nine
  source snapshots, including absence, storage identity, revision and height.
  Stale results are discarded and still-resident work is requeued.
- One delivery job and two halo jobs retain their input, count and readback
  scratch plus a monotonic fence. Input capacity grows only to the supported
  512-block height. Total retained scratch is bounded at 7,103,628 bytes. Each
  emitted mesh owns four fresh output buffers, so publishing another column
  cannot overwrite geometry still being drawn. At unchanged height this removes
  seven buffer creations and one fence creation per subsequent mesh job.
- Initial authoritative column delivery remains synchronous so the query view
  and displayed replacement agree before camera and target queries. It shares
  the staged upload implementation. This is not a fully asynchronous initial
  streaming pipeline; the existing final graphics-frame wait also remains.
- Pending statistics and capture/qualification gates include in-flight work.
  Mesh buffers and their sources remain owned until completion or safe teardown.
- `StreamResidency` scans for retired spatial payloads only after a window
  change. It detaches retired ownership under its mutex, then destroys those
  payloads on the worker after unlocking. The two-entry delivery queue and exact
  query-to-payload pairing are preserved.
- `WorldMeshInput` decodes contiguous central rows from lossless paged column
  storage. Uniform pages use fills; packed pages reuse their decoding metadata.
  The eight signed neighboring borders preserve the existing height mapping.
- Animated atlas frames cache their exact generated mip bytes at load time.
  Updates stage cached bytes and submit on the graphics queue without a separate
  host wait. The original animation timing, alpha and material rules remain.

No LOD, reduced distance, shader quality reduction, CPU rendering meshes or
network protocol changes are part of this repair. Original upload ownership was
consulted in the read-only reference's `world/chunks/upload_mesh.cpp`; first-party
rendering remains Slang through standalone SlangRHI.

The pinned RHI implementation copies upload bytes into retained staging handles
in `src/command-buffer.cpp` and retains submitted command buffers in the Vulkan
queue until completion. See the [standalone RHI source](https://github.com/shader-slang/slang-rhi/tree/e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc).

## Qualification

The final pooled package passes the canonical GPU/batch and CPU player/draw
qualification targets in `build/stream-reuse-qualified-{gpu,cpu,bundle}.log`.
The graphics run passes 708 animated GPU mip comparisons, exact
surface/material/fluid oracles, both batching paths and 512 oblique seam views
with zero interior holes. New cases cover
same-revision payload replacement, diagonal and absent neighbors, changed
vertical extents, eviction/reentry, empty replacement, bounded jobs and exact
animated GPU mip bytes. GPU validation reports zero errors and ten known
unused-varying warnings. CPU qualification includes 3,348 halo checks, 12,006
culling checks, 139 draw preparation checks and compact-storage/residency cases.

The installed package passes the shader/module/server/source equality and
833-file ownership/line-limit gates in `logs/client/stream-final-stage.json`.
Packaged render distance 4 -> 8 -> 4 reaches 81 -> 289 -> 81 columns with no
pending meshes before each transition. Domain UI tests pass 573 general and
1,175 inventory checks. The radius-4 GPU capture passes the surface validator
with 259,379 rectangles covering 1,355,786 unit faces. See
`logs/client/stream-final-distance*`. No OS input events were injected.

The pooled lifecycle fixture alternates 1, 8, 65 and 512-block heights and
enabled/empty material passes. Twenty focused repeats pass in
`build/stream-reuse-exact-fence-1.log` through `-20.log`. Qualification waits on
each submitted phase fence with a one-second cap; the longest observed wait was
11.0644 ms. The phase-pump bound is unchanged, and production polling uses a
zero timeout. Earlier queue-idle-based fixture runs intermittently exhausted
that bound: instrumented polls were called and returned timeout. The exact
fence waits establish completion directly. This is a qualification correction,
not evidence that a driver defect was diagnosed or fixed.

For reproducible runs with the actual display/material settings, use
`--benchmark-seconds 10 --benchmark-settings` and point
`OCTARYN_CLIENT_SETTINGS_PATH` at an isolated settings file. This preserves the
benchmark's disabled movement input, saved camera and full-residency warmup.
Without the new flag benchmarks retain their existing default settings.
`--benchmark-hidden` also requires a benchmark duration and creates a hidden
SDL window while preserving actual rendering and capture. It is useful for
unattended qualification, but hidden and visible runs must be identified when
comparing performance.

The GPU CSV now separates halo decoding, buffer allocation, upload staging,
command encoding, submission, readback, explicit fence waits, release and mesh
publication. These fields include initial deliveries before the graphics frame;
they are not subsets of the older in-frame `mesh_cpu_ms` field. The initial
unpooled profile was dominated by allocation and submission, motivating scratch
reuse. It did not establish an FPS improvement; final measurements must use
the pooled package and report their run conditions.

## Measured result

Fresh consecutive visible 1280x720 runs use radius 32, the same saved player
pose, right shoulder, benchmark input disabled and separate copies of the same
world files. Both finish with 4,225 columns, 15,223,536 mesh rectangles and
1,768 drawn columns / 6,224,767 drawn rectangles. The old executable was retained
unchanged (SHA256 `673BB91F22BED642A702278804DCC66AB0EC200E4B47C31CB84FEE9528801D29`).
The final native and installed executable hashes both equal
`E990090B2F7A2900F2DD65870D782A6C781A9CB18CE5A44DD216C04A89F06DAC`.

| Measurement | Before | After |
| --- | ---: | ---: |
| Full residency, periodic report bounds | 103.49-104.50 s | about 86 s |
| All boundary work complete | 120.67-122.67 s | about 96 s |
| Loading in-frame mesh CPU mean | 7.985 ms | 2.915 ms |
| Loading in-frame mesh CPU p99 | 17.564 ms | 4.043 ms |
| Initial delivery CPU, mean of report intervals | 6.929 ms | 1.590 ms |
| Settled GPU mean | 7.667 ms | 7.700 ms |

Backlog completion is roughly 20-22% earlier. Geometry is preserved and loading
CPU cost improves; settled GPU cost is essentially unchanged. Main/GPU report
joins match 132/132 before and 108/108 after. CPU frame samples are periodic,
not an all-frame FPS trace. Camera yaw/pitch/FOV were not captured for this pair;
the recorded pose/geometry and CLI camera configuration agree. Background Java
processes were present; this is a local comparison, not an isolated lab result.

At 2560x1440 with the user's material settings, a separate hidden run reaches
full residency in 87.65-88.65 s and finishes boundary work in 95.71-96.98 s.
Its final settled aggregate is 9.906 ms (about 101 FPS), with a 19.557 ms worst
frame across 1,010 samples. This is an absolute result, not a matched 1440p
before/after comparison. See `logs/client/stream-final-{before,after,1440}*` and
`work/stream-final-comparison.json` / `work/stream-final-1440-summary.json`.

Initial delivery and the final graphics-frame wait still synchronize with the
GPU. A 103.242 ms CPU encoding/wait outlier remains in the visible after run;
this repair does not eliminate every hitch. Remote multiplayer, Linux/macOS,
and sustained manual traversal were not qualified by these runs.
