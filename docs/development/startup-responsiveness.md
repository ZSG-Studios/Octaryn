# Renderer startup responsiveness

The prior boot progress callback pumped SDL only **between** synchronous device
and pipeline creation calls. A long compiler/driver call still blocked window
events for its entire duration.

Startup now submits one initialization job through the existing native scheduling
library. The main thread polls SDL throughout that job. Window-property access,
surface creation/configuration, startup presentation and failure teardown use a
synchronous worker-to-main handoff. The worker waits while those operations run;
this does not assume concurrent access to a Slang session, RHI device or queue is
safe. Normal rendering and RmlUi execution remain main-thread-owned.

The existing Slang RmlUi pipeline is initialized first. A temporary host loading
document presents the actual initialization stage through standalone RHI at
stage handoffs. There is no software/GFX renderer or estimated percentage. During
an individual compiler call the last loading image remains visible while SDL
events continue to be processed; loading frames are not concurrently submitted
against initialization. Startup documents shut down before the product UI starts.

Closing requests cooperative cancellation. The next stage boundary cancels
remaining work, and main-thread teardown is serviced before the window is
destroyed. A driver/compiler call already executing is not forcibly interrupted;
the title explicitly reports that shutdown is waiting for the current graphics
operation. No detached job retains a dead window.

One `client_boot responsiveness=1` summary records worker job count, event-pump
count, observed maximum event gap, worker elapsed time, total elapsed time and
ready/failed/cancelled outcome. Main-thread presentation/resize time is included
in the observed gap. This is a measurement, not a fixed responsiveness claim.

For visual qualification only, set `OCTARYN_CLIENT_BOOT_CAPTURE_PATH` to a BMP
destination in an existing directory. After the first successfully presented
loading frame's fence completes, the client reads its actual RHI color texture
and saves it using the same SDL BMP encoding path as world captures. Capture is
once per renderer lifetime; ordinary startup performs no boot readback or disk
write. Diagnostic readback time is included in that run's event-gap measurement.

## Checks

- `octaryn_startup_lifecycle_probe` exercises the production handoff/cancellation
  mailbox with the native worker scheduler and no SDL window or GPU. It checks
  worker execution, main-thread creation/teardown, deadline cancellation while a
  bounded stand-in driver call runs, and exception propagation from a failed
  main-thread operation. All three cases pass on Windows on 2026-09-18; log:
  `logs/tools/startup-lifecycle-20260918.log`.
- Changed C++ translation units pass clang-cl syntax checks. Integrated builds,
  GPU loading-image inspection and backend qualification remain separate from
  this CPU check.
- The coordinated 2026-09-18 Windows Vulkan radius-4 run exited successfully with
  180 world frames and 81 resident columns. Its startup summary measured one
  worker job, 738 event pumps, **106.61 ms maximum event gap**, 6619 ms worker
  time and 6622 ms total startup time. Evidence:
  `logs/client/startup-repair/startup-vulkan-zndqvl4d/client.log`. This establishes
  responsive event processing during that initialization run, not a 30 ms
  startup bound or fully smooth world rendering. Loading-image inspection uses
   the subsequent opt-in capture run; other platforms remain unqualified here.

## Combined runtime measurement

`logs/client/startup-repair/startup-vulkan-9unqwvd9/` records the rebuilt Windows
Vulkan client at 1280x720, natural terrain, radius 4, and an explicit 60 FPS cap.
It exited successfully after 180 measured world frames with 81 columns; ray
preparation settled at 81 ready columns and zero pending work. Startup recorded
931 event pumps and a 123.05 ms maximum gap over 8534 ms, including diagnostic
loading-image readback. The display refresh query ran once over 303 paced frames.
Per-frame CSV measurement gave 38.525 ms median and 42.046 ms p95 over 82 late
settled samples; this is **not 60 FPS or smooth-world acceptance**.

Actual loading and both world captures were inspected. The loading text wrapped
into a narrow top-left column because the temporary document lacked full-size
body bounds; explicit body bounds were subsequently added and need a new capture.
World captures show very dark foliage/terrain. The user additionally reports
flickering and artifacts; visual repair now takes priority over startup tuning.
The original radius-16 ray-preparation failure remains unproven fixed.

The corrected loading document was rebuilt and its actual RHI image inspected in
`logs/client/startup-repair/visual-vulkan-0-native-nywpnyrj/startup-loading.png`.
The heading, real initialization stage and explanatory text now occupy separate,
readable lines within the window. Explicit body bounds and block display are
required by this temporary document's standalone RmlUi stylesheet.
