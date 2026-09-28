# Renderer retirement profiling

Build35 source adds an opt-in CPU retirement trace. It does not establish the
cause of build33's Vulkan spikes: one frame contains a measured 61.9922 ms slot
wait, while another contains 18.0019 ms inside rendering not assigned to the
older CPU fields. Neither observation identifies driver execution, OS scheduling
or disk I/O as its cause.

`capture_map_world.py --frame-cpu-trace` activates schema1 and writes
`frame-retirement.csv`. Native activation is
`OCTARYN_CLIENT_FRAME_CPU_TRACE=1` with
`OCTARYN_CLIENT_FRAME_CPU_TRACE_PATH`; an omitted path derives from
`OCTARYN_CLIENT_GPU_PROFILE_PATH`. Invalid activation or missing output fails
explicitly. Production defaults off. The capture helper requires the activation
marker, complete ordered records and an exact completed-world-frame join to
`gpu.csv`. Existing captures without the option do not request this schema.

The renderer owner records up to128 primary intervals and4 fence retirements per
call. Each fence has slot, target value, source renderer frame, completed values
before and after, actual wait invocation, result and three nested wall spans.
The actual retirement state machine supplies these identities; they are not
inferred by subtracting two from the current frame. An empty slot uses
`UINT64_MAX` as its unknown source. A queue-wide synchronization submission may
have that unknown source intentionally. Already-completed and empty slots emit
zero wait time. Failed waits never invent a successful postcheck.

Primary spans include SDL/window checks, resizing, initial frame retirement,
lighting/GPU/temporal query resolution, ray-counter resolution, command recording,
submission, presentation, optional serialized retirement, capture, and destruction
of renderer-local objects before returning to the wrapper. Diagnostic GPA polling
has a separate span after the ordinary fence wait. Successful minimized and
null-acquire paths are explicitly skipped/abandoned. Failed or unwound frames
invalidate the requested trace. Nested fence spans must not be added to primary
spans. Trace formatting/serialization and final health checking happen after the
terminal wall endpoint; diagnostic overhead is excluded from that endpoint and
these captures never qualify ordinary frame performance. Window-title/profile
work in `WorldProfile::frame` remains outside the renderer wrapper.

`GetThreadTimes` has coarse granularity. Its samples cannot establish precise
active execution, and a timed fence call alone cannot distinguish GPU work from
driver scheduling or a delayed CPU wakeup. A forcibly terminated pending frame
may have no terminal record; missing records and frame joins invalidate the
capture, rather than attributing the unfinished interval. This trace adds no
timestamp coverage for private worker GPU initialization.

The main GPU, lighting and per-frame CPU CSVs already used
`Diagnostics/AsyncProfileStream.h`. Build35 retains that implementation and moves
the remaining ray diagnostic CSV and once-per-second world summary onto it.
Formatting stays with the owner; a bounded64×16KiB byte queue transfers ordered
bytes to a worker that alone writes/flushes the file. Saturation fails capture
instead of blocking a frame or silently dropping records. Shutdown joins and
closes outside normal rendering. Worker write, queue and close failures emit
`profile_writer_failed capture_invalid=1`; renderer teardown also checks final
drain/close results. Mid-session drains during startup/resize never close writers.

CPU-only evidence: `logs/build/build35-frame-retirement-probe.log` and
`logs/tools/frame-retirement-probe.json`. The native fixture exercises actual
ordered output for10000 rows, deterministic bounded-queue saturation, injected
worker-write/close failures, seven fence state paths, three terminal frame states
and interval overflow. `test_frame_retirement_report.py` has four focused tests
for missing/damaged records, frame joins, nested accounting and unsafe retirement.
Touched renderer/application translation units pass syntax checks in
`logs/build/build35-frame-retirement-syntax.log` and
`logs/build/build35-world-profile-syntax.log`.

Build35 canonical `octaryn_all` and both real renderer trace captures passed.
Cases are
`logs/client/hq200-build35/dx12/trace/retirement/map-dx12-on-0-ob5b2p5p` and
`logs/client/hq200-build35/vulkan/trace/retirement/map-vulkan-on-0-34sxkyfb`.
The independent report is
`logs/client/hq200-build35-retirement-analysis.json` with a Markdown companion
and separate complete per-frame artifacts. It validates all1600 camera-ready
renderer/CPU/GPU/trace joins per backend, actual poses and720internal/1440output
dimensions. No incomplete or abandoned records occur. All1600 frames and spikes
remain in the primary report; the declared ready>=120 window is also reported
separately for1480 frames.

| Diagnostic scope | DX12 wall p99 / worst | Vulkan wall p99 / worst |
|---|---:|---:|
| All1600 ready frames |12.386275 /16.929800 ms|11.720651 /14.096900 ms|
| Separately declared ready>=120 |7.815129 /8.547300 ms|6.681126 /7.267600 ms|

The earlier48–64 ms Vulkan stalls did not reproduce. The worst steady DX12
frame1380/ready1013 spends7.7463 ms inside the actual fence call for value1396,
source frame1378 (completed1395→1396). Vulkan frame1111/ready790 spends5.8543 ms
in the call for1127, source1109 (completed1126→1127). Query-resolution wall spans
are0.0318 and0.1278 ms respectively, cleanup0.0020 and0.0019 ms. Source GPU rows
are retained as context only; their durations are never subtracted from a wait.

Maximum unassigned time inside the trace is0.1965 ms DX12 and0.2134 ms Vulkan.
The separate outer render-minus-trace residual peaks at0.3232 and0.3668 ms in
the steady window; it includes recorder serialization and boundary differences.
Across all ready frames timestamp-query resolution peaks at0.2817/0.3557 ms.
The Vulkan first-frame0.7271 ms ray-counter begin span is reported separately:
collection is disabled, so this is counter setup, not counter readback. Query
resolution spans include their owner CPU processing as well as backend reads.

These are diagnostic runs with additional CPU sampling and record formatting,
not ordinary FPS or HQ200 budget qualification. They neither attribute the old
18 ms gap to disk I/O nor prove that its cause was fixed. Hardware device-loss,
null-acquire and precise active CPU behavior are not established by these runs.
