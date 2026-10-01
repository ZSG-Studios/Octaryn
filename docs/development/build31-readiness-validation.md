# Build31 focused startup-readiness validation

The build31 Vulkan capture passed exit zero with RHI validation at
`logs/client/hq200-build31-readiness/validation/vulkan/tiles-vulkan-a27r_iqi`.
The command below is the focused reproduction recipe. Run it after a canonical
build completes, while other builds and GPU work are idle.
The control is build30 Vulkan case
`logs/client/hq200-build30-tiled-as/validation/vulkan/tiles-vulkan-n3bcf4cv`.
Its 299-tile lossless manifest and explicit budgets are retained.

```powershell
python tools/validation/capture_tile_world.py `
  --client-bundle-root build/release-windows/client/bundle `
  --evidence-root logs/client/hq200-build31-readiness/validation/vulkan `
  --manifest build/release-windows/client/map-variants-hq200/lossless/map.json `
  --backend vulkan --performance-profile HQ200 --width 2560 --height 1440 `
  --smoke --require-all-tiles --startup-readiness --rhi-validation `
  --uncapped-fps --frames 2400 --capture-ready-frame 120 `
  --gpu-budget-mib 4096 --collision-budget-mib 512 `
  --draw-mode direct --texture-reuse on --as-inflight 4 `
  --timeout 300 --max-frame-ms 50
```

This is a hidden bounded startup capture with an isolated world, saves, settings,
inventory and server logs. The helper removes inherited qualification variables.
RT stays required; HQ200 stays active with reconstructed 2560x1440 output. This
is not a native-resolution performance measurement. The existing watchdog keeps
its two-second frame-heartbeat stall limit, map-ray startup limit and owned-process
teardown; the overall deadline is explicitly 300 seconds. Do not increase a limit
to turn a failure into a pass.

Inspect the unique case path printed by `tile_capture_started`; preserve its raw
logs and result even on failure. Required checks:

- `result.json` reports exit zero, RHI validation enabled, HQ200 active, dimensions
  `[2560,1440]`, 299 published tiles, startup-only scope and startup diagnostic
  activation. The helper rejects Vulkan validation errors, damaged measurements
  and missing capture evidence. Inspect the captured image too.
- `client.log` contains the exact activation marker:
  `startup_readiness_profile enabled=1 clock_origin=main_entry cadence=every_successful_world_present cpu_scope=frame_total`.
- Both `world_initial_playable_candidate` and `world_initial_playable` are observed
  for the initial map session. Candidate can precede RT completion. Their elapsed
  times and actual zero-based renderer frames must be ordered; requested-generation
  and requested-set hash are recorded identities, not assumed identical across the
  two events when publications or requests changed.
- Final event has `schema=1`, `clock_origin=main_entry`,
  `scope=all_manifest_rt_guard`, `ray_required=1`, `ray_ready=1`,
  `ray_guard_complete=1`, `collision_ready=1`, `requested_ready=1`,
  `visible_missing=0`, and `resident_tiles=total_tiles=299`. Requested count is
  positive and at most 299; all visible tiles are ready. Actor coordinates are
  the actual authoritative pose, not a camera-only position.
- Corroborate final renderer frame against `frame-timing.csv` and `gpu.csv` where
  their capture windows cover it. The event requires a successful world present;
  an abandoned/null-acquire attempt cannot substitute. A pre-profile event may be
  valid without a GPU row; record that coverage limitation, do not invent a join.
- Read `tile-performance.json` → `tile_work.initial_playable_startup`. It retains
  activation, both events and final app-main elapsed time separately from full-map
  request/residency statistics. Absent events remain unknown even if all tiles
  published. Activation alone is not successful readiness qualification.

A final conservative guard at or below 5,000/8,000 ms proves only that app-main
bound. A later all-manifest guard is
`conservative_readiness_gate_exceeded_initial_playable_unqualified`, not proof
that the earliest required playable subset missed its budget. Process-launch
latency remains null because native main excludes OS loader/bootstrap time.
Instrumentation scans count toward frame CPU; RHI validation and this opt-in
diagnostic run cannot be substituted for quiet performance triplets.

After the capture, run the existing CPU-only accounting audits against its exact
case path:

```powershell
python logs/tools/audit_external_as.py PATH_TO_CASE
python logs/tools/audit_abandoned_frames.py PATH_TO_CASE
```

This establishes normal Vulkan queue/initialization integration after the terminal
cleanup patch, not an actual GPU device-loss recovery test. Device-loss, reset
failure, resource lifetime and poisoned-slot reuse remain the focused fake-Vulkan
fixture's scope. Private backend initialization GPU duration is still outside
schema4 main-plus-explicit-AS timing; no complete streaming GPU-budget claim.

No additional non-RT case is scheduled: this helper forces required RT and HQ200
also forces RT. Disabling it would invalidate the intended contract. Existing
parser tests cover non-RT event semantics but do not constitute native runtime
coverage. The separately scheduled build31 DX12 native quality comparison covers
basic runtime on that backend; neither short capture is full gameplay acceptance.

## Observed build31 result

Activation was explicit and both candidate/final events were observed in session1
at **28,966.132 ms from native main**, renderer frame1526, authority acknowledgement726.
Both recorded publication generation299 and requested-set hash1860860489255566541.
Requested/resident/total counts were299; 65 tiles intersected the guarded view,
with zero missing. Requested, collision, ray-required, ray-ready and RT-guard flags
were all1. The authoritative actor and camera were `(0,1.938856,-20)`, yaw0.6,
pitch-0.25, vertical FOV1.570796 radians. Equal candidate/final timestamps are
valid: this particular all-requested workload first satisfied both observations
in the same successful present.

Frame1526 exists exactly once in each CPU and GPU CSV. The CPU file has2431 rows,
the GPU file2400 rows. The later capture at frame1590 independently records all299
resident/wanted, generation299 and no preparing/uploading tiles. Raw stdout has no
abandoned-frame marker. Both subsequent accounting audits pass, with receipts
`logs/tools/build31-external-as-vulkan-readiness.json` and
`logs/tools/build31-abandoned-frames-vulkan-readiness.json`. All2400 GPU rows
resolve, with528 external AS operations (264 builds and264 compact copies);
70 earlier AS submissions fall outside the renderer profiling window. The
598 logged submissions match that decomposition. Ordered operation kinds match
with a constant951-frame schedule offset; this corroborates correspondence but
is not a directly emitted shared frame identifier. Main-plus-AS and stage-sum
identities hold within2e-15ms rounding error. No missing interior GPU records,
duplicate CPU records or query errors were observed. Private initialization GPU
duration remains uninstrumented, so complete streaming GPU budget coverage is
explicitly false in both receipts.

The strict report correctly records
`conservative_readiness_gate_exceeded_initial_playable_unqualified`; both 5s/8s
budget results and process-launch latency remain null. This establishes the new
event's normal Vulkan runtime integration and explicit conservative readiness,
not an initial-playable budget pass or failure. The root capture owner inspects
the actual image separately and found the captured street complete. No hardware
device loss was induced, and no normal
gameplay, cold/warm triplet, sustained FPS or full acceptance claim follows.
