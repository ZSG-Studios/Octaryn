# Player movement and F3 recovery

Current repair: 2026-09-13. The client remains on standalone Slang RHI and
Slang-authored shaders. These repairs use the existing local authoritative
session and Jolt character owner; they do not introduce client-authoritative
movement or claim remote multiplayer qualification.

## Confirmed causes and fixes

- Player publication restarted a 1/60-second interval after each write. A 16 ms
  authority loop therefore published every second iteration. Publication now
  preserves its deadline phase, publishes only new source ticks, and acknowledges
  successful file replacement. Failed writes remain retryable; missed slots do
  not cause catch-up bursts.
- Host-only process ticks advanced the player/world without advancing published
  source identity. The same successful tick owner now advances both clocks.
- PoseHistory treated an exact valid endpoint as an underrun and could wait for
  another 50 ms of snapshots unnecessarily. Only an actual overrun now enters
  refill. Source playback remains 1x, with no arrival-driven speed adjustment.
- The Windows host used a relative sleep after work, including coarse scheduler
  rounding. A blocking high-resolution waitable timer now follows absolute
  deadlines. Creation/wait failures propagate; there is no busy-spin loop or
  catch-up simulation burst. Other platforms use steady-clock sleep_until.
- SessionIo also used a coarse Windows timed condition-variable wait. Its private
  high-resolution timer now preserves the 16.667 ms phase and has an interruptible
  stop event. Blocked file work skips missed polls; input expiry, latest-only
  delivery, queue bounds and duplicate suppression are unchanged.
- Rounded, separate voxel collision boxes caused periodic floor seam contacts.
  Sprinting covered 88.8441 instead of 90 blocks in ten seconds. Sharp cubes in
  one static compound preserve the exact voxel union and let Jolt's enhanced
  internal-edge removal discard hidden contacts. This follows the pinned Jolt
  Architecture.md guidance for neighboring boxes. Character movement, gravity,
  wall/ceiling collision and authoritative ownership remain in their existing
  owners.
- F3 replaced the complete 19-row metrics subtree every refresh, rebuilding
  unchanged labels and units. Rows now retain their text nodes and update changed
  values. Small immutable UI geometry uses RHI Upload memory, avoiding separate
  initial-data transfer submissions. The current pinned dependency's DeviceLocal
  upload path is already asynchronous: synchronous transfer waits are not a
  proven cause of these measurements.
- Ordinary gameplay mouse/key events also ran menu synchronization despite
  making no UI change. They now return through the existing uncaptured-event
  path. Frame updates skip hidden menu/lighting synchronization while preserving
  startup, visible panels and the final close-transition update.

## Verification recorded so far

`build/movement-final-probes.log` executes actual Windows probes:

- Native movement: walk 49.9997 and sprint 90.0006 blocks per ten seconds;
  64 walk/sprint cases across eight cardinal/diagonal directions and
  30/60/120/variable update intervals. Every measured step checks floor height;
  room corners, ceilings, held jump and existing collision checks pass.
- Native host loop: 128 intervals, median 16.034 ms, p95 16.311 ms,
  elapsed 2.048050 seconds. Shutdown and callback failure tests execute too.
- Actual RmlUi document/font/layout with a mock render interface: 400 refreshes,
  23,600 old geometry compilations versus 2,400 retained-value compilations;
  128.204 versus 94.9834 ms CPU time, with zero live geometry after teardown.
  This fixture does not create a GPU device.

`logs/server/movement-ui-probe.log` passes the managed production publication
clock (600 writes in the simulated ten-second 16 ms loop), actual native-planned
host-only tick clock, publication retries, bounded replication and fluid/save
regressions. `build/movement-ui-bundle.log` passes the production PoseHistory
cadence/contact/endpoint/refill tests. Normal 30/60 Hz delivery with bounded
jitter and 60/240/1000 Hz playback retains the 1x source clock.

`logs/client/movement-network-final.log` runs the real LocalSession and packaged
server against a fresh isolated world. Its measured five-second flight has zero
holds/underruns, equal wall/source elapsed time, speed p50 9.999996 and p95
10.000285 for authoritative speed 10. This uses domain-level input, not OS input
injection. This initial run predates the worker timer repair.

`build/movement-session-io.log` subsequently passes the actual production worker:
120 polling intervals, median 16.6126 ms, p95 17.0856 ms; stop-event wake 0.0518 ms.
Stalled-read/latest-input/250 ms expiry/no-heartbeat/shutdown checks pass, with no
file I/O on the frame thread. `logs/client/movement-network-precise.log` then runs
the production LocalSession with the packaged authority: zero holds/underruns,
4.997630 seconds of both wall/source playback, speed p50 9.999996 and p95
10.000262 for authoritative speed 10. Presented bracket intervals improve from
31.063 ms median to 15.536 ms, with 53.400 ms buffered at completion.

## Packaged F3 measurements

RX 9070 XT, 1280x720, same fixed view, 81 resident columns and 1,483,600 quads.
Each run has five seconds warmup and ten measured seconds; graphics validation
is off for timing. These are local fixed-view samples, not gameplay guarantees.

| Log under logs/client | Mean frame ms | Histogram 1% low FPS | Worst ms |
| --- | ---: | ---: | ---: |
| movement-ui-before-off | 0.918 | 666.67 | 2.772 |
| movement-ui-before-on | 1.396 | 35.71 | 59.916 |
| movement-ui-after-on | 0.925 | 500.00 | 11.318 |
| movement-ui-final-off | 0.740 | 666.67 | 4.986 |
| movement-ui-final-on | 0.964 | 500.00 | 30.973 |
| movement-ui-verified-profile | 0.804 | 571.43 | 11.122 |

The final-on outlier includes 28.144 ms inside UI update and remains under
investigation. An intervening off run, `movement-ui-after-off`, recorded a
3,158.195 ms frame with 3,156.204 ms in SDL event processing; it is retained in
the evidence rather than silently discarded. Improved average/low-percentile
results do not establish that every hitch is resolved. Other staged server
repairs were installed between the original and repaired package runs.

The verified-profile run includes opt-in OCTARYN_CLIENT_UI_PROFILE stage timing.
After removing hidden-panel work, its maximum UI update is 0.508 ms including
startup, and 0.371 ms for the slowest retained later sample. Menu/lighting work
is zero in the recorded gameplay updates. The 11.122 ms worst whole frame is
therefore not a corresponding UI-update spike. The optional profiler retains
only eight samples and reports at teardown; Windows thread CPU time has coarse
accounting precision, so zero CPU time is not proof of no CPU work.
The previous 28.144 ms UI outlier was not reproduced in this final run. Occasional
render-stage spikes still require separate investigation.

`movement-ui-validation.log` and `movement-ui-validation-error.log` pass the
packaged 543-check UI contract, 4,348 resident-world frames, actual third-person
player/F3 screenshot and RHI/Core/Synchronization validation with zero errors or
warnings. Live UI geometry stays at 78 for frames 1, 301 and 601 while changed
values compile new geometry. This GPU run predates the final worker/host timing
qualification; its capture is not a moving-camera networking test.

The coherent package installation succeeded after the previous user processes
had exited normally. Saved worlds and backups were not migrated or changed by
these isolated tests. Full remote-network impairment/latency qualification,
short discrete-input delivery, natural vegetation wiring, and remaining engine
feature parity are separate outstanding work.

Final installation: build/movement-ui-verified-bundle.log installs the coherent
package; logs/client/movement-verified-stage.json qualifies the actual installed
151-file bundle, shader equality, module/native payloads, owner boundaries and
source line limits. logs/server/movement-verified-probe.log repeats the managed
publication, persistence, bounded replication and fluid integration tests after
the final native physics/timing changes, with successful exit status.

Final GPU run: movement-ui-verified-gpu.log and its -error.log pass 550 UI contract
checks and 4,444 resident frames with zero RHI/Core/Synchronization warnings or
errors. The contract directly exercises 8,192 ordinary gameplay motion events
in 0.163 ms without OS event injection, plus gameplay-key and F3 routing. The
renderer screenshot shows terrain, player, HUD and F3 together; it is captured
during validation warmup and is not an FPS benchmark image.
