# Initial playable readiness evidence

Build31 source adds observation only; loading-screen and input policy are unchanged.
It is disabled by default. Explicit `OCTARYN_CLIENT_STARTUP_READINESS=1` enables
the diagnostic; `0` disables it, and other nonempty values fail configuration.
The enabled profile scans the manifest each successful world present until the
final gate. Its work appears in whole-frame CPU time and must be disclosed in
timing evidence. `startup_readiness_profile` records activation and cadence.
Disabled runs emit no synthetic readiness value. This avoids ongoing manifest
scans in ordinary partial-residency worlds where the conservative guard may
never become true.
The clock begins at the first statement of native `main`, before argument parsing
and SDL initialization. `clock_origin=main_entry` excludes OS process creation,
loader work and static initialization. It includes any menu dwell before opening
a world. A main-entry bound is not a process-launch bound.

`world_initial_playable_candidate` is emitted once per map session after all of:

- A successful world render advances the renderer frame counter. Minimized,
  menu-only and null-acquire attempts cannot qualify.
- The session supplies a latest authoritative pose, and the client's owned
  prediction collision source reports that actual position ready. This is not
  merely the manifest spawn, which can differ from a restored player position.
- Every currently requested tile is published Ready, with no preparation/upload
  work outstanding. Publication follows collision installation and required map
  ray preparation.
- Every manifest tile whose declared bounds intersect the current raster frustum
  is Ready. The existing map visibility camera supplies the same projection and
  far plane, expanded conservatively by one pixel for temporal jitter. Thus a
  potentially visible tile outside the radius-based requested set prevents the
  event rather than being assumed invisible.

`world_initial_playable` additionally requires the requested RT scene to have
complete current coverage, no pending jobs, and every resident map ray-ready.
There is no proven bounded set of offscreen reflection/shadow dependencies yet.
Therefore RT uses `scope=all_manifest_rt_guard`: all manifest tiles must be Ready.
Without RT, scope is `visible_requested_region`. Missing guards leave final
readiness unknown; neither authority readiness nor the first map alias can
substitute. Monolithic maps represent one complete geometry asset.

Both events record the zero-based renderer frame, map-session ordinal, authority
acknowledgement, actual actor/view, wanted tile-index hash, resident generation
(`requested_generation` field), tile counts and guard flags. The current view is
the presented view, including an explicitly requested camera qualification route;
it is not implicitly the original manifest camera. Generation describes
publication, while the hash separately identifies the current requested set.
Asset/manifest identity remains in the capture's recorded build and asset metadata.

The event follows successful RHI present submission; it does not timestamp an OS
compositor scanout. It imposes no extra GPU wait and no 64-frame temporal
convergence delay. Required visual quality still needs image/motion verification.

A timely final conservative guard can establish that app-main-start bound. A late
whole-map superset cannot establish that the earliest required playable subset
missed a budget. Reports therefore label a late event
`conservative_readiness_gate_exceeded_initial_playable_unqualified`, preserve
the earlier candidate separately, and leave process-launch latency null.
Historical captures without these events remain unknown. Full-manifest request
latencies and final residency statistics remain separate evidence.

The five affected translation units passed targeted native syntax checks
(`logs/build/build31-readiness-app-syntax.log` and
`logs/build/build31-readiness-render-syntax.log`); all eight strict reporter tests
passed. Canonical build31 and its Vulkan299-tile RHI capture also passed; see
`build31-readiness-validation.md`. That run emitted the conservative guard at
28,966.132ms from main entry, correctly leaving earliest-playable 5s/8s budgets
unqualified. No historical result is relabeled.
