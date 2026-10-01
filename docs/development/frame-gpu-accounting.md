# Frame GPU accounting

The main frame encoder is recorded before a tile pump, but submitted after it.
Map BLAS construction and compaction use independent command buffers submitted
by the pump. Timestamps placed only in the deferred main encoder therefore did
not include that AS work. This affected historical `gpu.csv` total/streaming
measurements and production dynamic-resolution feedback during those frames.
Static frames with no independent AS work retain their original measured scope.

The source prepared for build29 reserves an additional timestamp pair in each
existing frame query pool: indices 10/11 in `WorldGpuProfile` and 2/3 in
`TemporalTiming`. `MapRaySubmitScope` brackets actual independent AS commands,
including build and compact-copy/TLAS submission. Allocation-only work does not
produce a GPU interval. `WorldAsTiming` sends the same scope to the optional CSV
profiler and the production DRS timer. DRS therefore includes AS work even when
CSV profiling is disabled.

The owner permits at most one such GPU operation per frame. The independent
command buffer uses the same queue and is submitted before the main command
buffer. The main frame fence thus covers both; query slots reset only after
their frame slot retires. Main query results are resolved first. An external
range is read only after its nonblocking state check reports `Resolved`; an
unwritten range is not read. Descending timestamps, an unclosed pair, and an
external interval overlapping the main interval fail rather than supplying a
partial total. No extra queue wait or submission is introduced for profiling.

GPU CSV schema 4 preserves the old columns and adds `main_gpu_ms`,
`external_as_ms`, `external_as_submissions`, `external_as_kind` (0 none, 1 build,
2 compaction), and `external_as_covered`. `total_gpu_ms` is main duration plus
exclusive external duration. `streaming_ms` includes that same external
duration exactly once. The sum excludes an idle gap between the two submissions;
it measures GPU work intervals rather than process startup or end-to-end frame
latency. CPU frame timing remains separate.

Schema 4 does **not** establish complete streaming GPU cost. Backend-private
initialization, such as Vulkan default-layout transitions, buffer initialization
copies and texture init-data submissions, is not bracketed by `MapRaySubmitScope`.
Those operations can execute outside both recorded spans while asset workers
prepare tiles. Same-queue ordering and lifetime fences do not measure their
duration. `external_as_covered=1` describes explicit map AS coverage only; it does
not cover private initialization. DRS uses the same limited sum. Startup remains
separate, and active streaming GPU-budget acceptance stays open until this work
is attributed. Incidental contention within a measured span is not a substitute
for measuring missing operations; neither zero cost nor an idle-inclusive queue
envelope should be presented as their GPU execution cost.

`hq200_budget.py` validates these identities and reports the AS distribution and
build/compaction counts. Historical schema 3 remains readable but cannot qualify
the complete streaming GPU budget. It must not be relabeled as complete simply
because its reported main span met a threshold. Loading before the renderer
frame loop also remains separate from frame timing.

## Verification status

`python tools/validation/validate_frame_timing.py` compiles a CPU-only fixture
against the actual production timing headers. It verifies main-plus-AS DRS
accounting, static frames, compaction, unused queries, unresolved ranges without
blocking reads, invalid intervals, failed query reads, and the one-operation
guard without either profiler. It passed; evidence is
`logs/build/build29-frame-timing-probe-3.log`. This is query-accounting evidence,
not actual GPU execution. The header compile emitted the existing `getenv`
deprecation warning.

The 11 HQ200 budget tests and four performance CSV tests also passed. Canonical
build29 passed. The DX12 tiled validation case
`logs/client/hq200-build29-as-smoke-dx12/tiles-dx12-5j70atzt` contains 2,400 GPU
rows and 528 independently timed AS submissions (264 builds, 264 compactions).
`logs/tools/build29-external-as-dx12.json` checks all timing identities and the
ordered schedule correspondence; another 70 submissions occurred before the
frame CSV window. No unwritten-query or descending-timestamp failures occurred.
This instrumented graphics-validation run does not qualify performance. Vulkan
failed concurrent queue access in build29, so it does not establish parity.

## Swapchain acquisition and abandoned frames

The build30 source places independent AS submission before swapchain image
acquisition. Vulkan acquisition reserves wait/signal semaphores for the next
normal queue submission; placing a tile AS submit between acquisition and the
frame submit incorrectly consumes that reservation. Backend initialization
submissions must separately bypass surface synchronization.

If acquisition returns no image, the already-recorded atlas/tile uploads are
submitted with the active frame-slot fence and acknowledged before resizing.
Dropping that encoder would lose uploads whose builder offsets already advanced.
The existing bounded resize drain covers both external AS queries and uploads
before slot reuse. Incomplete frame timestamp ranges are not published to CSV
or DRS; `world_frame_abandoned` records the omitted frame work explicitly.
Fatal early exits after external work synchronously retire that queue work with
a bounded fence guard. Successful normal frames add no guard wait or submission.
The existing `acquire_cpu_ms` bucket continues to include temporal preparation,
tile pumping, and acquisition; it is not a pure WSI wait measurement.

The production guard's CPU regression checks no-work, successful slot ownership,
failure before main submission, and failed retirement. It passes in
`logs/build/build30-frame-timing-probe.log`. Actual swapchain resize/null-image
and Vulkan graphics validation remain runtime qualification work.

Build30's 299-tile DX12 and Vulkan RHI controls both passed. Cases are
`logs/client/hq200-build30-tiled-as/validation/dx12/tiles-dx12-2r4vp_su` and
`logs/client/hq200-build30-tiled-as/validation/vulkan/tiles-vulkan-n3bcf4cv`.
Each has 2,400 contiguous GPU rows with 528 explicit AS spans: 264 builds and
264 compactions. Ordered schedule events match each span; 70 earlier AS
submissions belong to startup. Timing identities agree within 4e-15 ms, with
no query failures, abandoned markers, duplicate CPU IDs or interior GPU gaps.
Receipts are `logs/tools/build30-external-as-{dx12,vulkan}.json` and
`logs/tools/build30-abandoned-frames-{dx12,vulkan}.json`. These establish separate
backend correctness evidence for explicit AS timing. They do not exercise the
null-image branch, measure private initialization, or qualify FPS.

Build31's Vulkan299-tile readiness/RHI case
`logs/client/hq200-build31-readiness/validation/vulkan/tiles-vulkan-a27r_iqi`
also passes both accounting audits. Its2400 GPU rows contain528 explicit AS
spans (264 builds/264 compactions), with70 earlier submissions outside the
renderer window. Schedule correspondence uses the observed constant951-frame
offset and operation kinds, not a directly emitted shared frame ID. Identities
agree within2e-15ms; no query errors, duplicate CPU IDs, interior GPU gaps or
abandoned attempts occur. Receipts are
`logs/tools/build31-external-as-vulkan-readiness.json` and
`logs/tools/build31-abandoned-frames-vulkan-readiness.json`. The opt-in startup
scan and RHI validation preclude a matched performance claim. Private GPU
initialization remains uncovered; null-image and hardware device-loss recovery
are not exercised by this successful run.
