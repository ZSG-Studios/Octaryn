# D3D12 presentation policy

The current renderer remains Slang-RHI; this repair does not implement the
planned NVRHI cutover. The registered `slang-rhi-d3d-flip-presentation.patch`
repairs the shared DXGI surface policy without changing rendering quality.

With vertical sync disabled, the surface queries `IDXGIFactory5` for
`DXGI_FEATURE_PRESENT_ALLOW_TEARING`. Only supported flip swapchains receive
`DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING`; their windowed presentations use
`Present(0, DXGI_PRESENT_ALLOW_TEARING)`. Unsupported devices retain `Present(0,0)`.
Vertical sync remains `Present(1,0)`. An exclusive fullscreen swapchain is
checked before presenting and never receives the tearing flag.

The unused `FRAME_LATENCY_WAITABLE_OBJECT` flag is removed. The client already
limits outstanding graphics work using its two frame completion slots; it did
not consume DXGI's additional waitable object or configure that object's default
one-frame latency. Microsoft describes the parallelism tradeoff in its
[waitable swapchain guidance](https://learn.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains).
The feature and presentation conditions follow Microsoft's
[flip-model guidance](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model)
and [DXGI_PRESENT contract](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present).

The OpenFNV visible client reproduced 1,016ms and 1,244ms frame-fence waits after
resizing to 2560x1369. A subsequent live GPU profile recorded a 236ms wait with
approximately 1.5ms rendering work; a fresh hidden run at the same camera and
resolution measured a 1.545ms median and 1.778ms maximum GPU frame over 500 warm
frames. These observations isolate a completion/presentation stall rather than
an expensive shader, and do not alone prove this policy repair fixes every hitch.

`python tools/validation/check_d3d_flip_presentation.py` compiles the production
policy helper and checks all supported/unsupported, flip/blit, vsync, and
exclusive fullscreen combinations without a GPU. Registered patch replay must
also pass. Qualification of the user's symptom additionally requires matched
resize and motion runs and observation of the visible client after rebuilding.

Hidden benchmark rendering now uses offscreen GPU targets. The renderer latches
an initially hidden SDL window (the `--benchmark-hidden` launch path), and bypasses
swapchain configure, acquire, framebuffer copy and Present throughout startup,
world/menu frames and retirement. Color/depth geometry, HDR, UI, frame fences,
watchdogs and GPU texture readback remain active. Later hiding an ordinary visible
window does not switch its rendering mode. The runtime reports
`hidden_offscreen=1 visible_present_qualified=0`; these captures qualify GPU output
and CPU/GPU workload, not desktop presentation, resize or visible frame pacing.

This distinction is necessary because a serial hidden 960x540 pause capture in
OpenFNV `logs/interactive-ui-pause-small-final` still recorded 200–1022ms completion
waits after the presentation-policy patch, while retired GPU ranges took roughly
0.31–0.37ms. The candidate executable matched the canonical bundle SHA256 and
linked the refreshed static RHI library. Removing desktop presentation from hidden
captures isolates that dependency; it does not establish the exact desktop or
driver scheduling cause or claim that the user's visible FPS symptom is repaired.
Microsoft documents that flip swapchains do not return
[DXGI_STATUS_OCCLUDED](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-status),
so a successful Present alone cannot identify an unoccluded desktop workload.

The subsequent registered `slang-rhi-d3d12-surface-retirement.patch` fixes an
independent image lifetime error. The prior D3D12 surface registered each acquired
image's event for the old fence value, then incremented and signalled a different
value after Present. Its event could therefore retire against previous work.
The new production helper reserves the next value, registers that exact value for
the recorded acquired image, Presents, and signals the same value with checked
HRESULT handling. Registration, Present, Signal or overflow failure invalidates
the surface and releases its image event; further acquire fails until a caller
configures it again. This is a correctness repair, not proof of the FPS cause.

Configuration now logs actual swapchain flags, buffer count, VSync, the tearing
query result and permitted windowed Present flags. It logs on presentation policy
changes (including startup VSync to active Immediate), not every frame or resize.
The focused checker replays both patches canonically against the pinned source
and compares the active SDK, then runs 24 presentation and 33 retirement assertions
without a GPU. Retirement fixtures cover uncompleted images, ordering, exact
registration/signal values, every failure boundary, overflow and nonfailure DXGI
statuses. The legacy broad Vulkan overlap-validation failure remains separate.

The Goodsprings visible client subsequently failed at frame 3830 in
`surface_acquire` after 2000.090ms. The preceding frame-queue retirement completed
in 14.705ms. Frame 3828 spent 695.773ms acquiring an image despite measured GPU
work of 5.055ms. The original files and a hashed, bounded tail are preserved in
OpenFNV `logs/goodsprings-visible-acquire-audit-v1`. The process had already exited;
the audit did not close it or generate input. Those logs lack the surface's exact
per-image target and completion values, so they cannot distinguish an unset event
after completion from a genuinely pending queue Signal.

The new registered `slang-rhi-d3d12-surface-completion.patch` records the last
successfully queued retirement value for each image. Acquisition uses the fence
predicate before and after waits; a completed image never waits on an unset event,
and a signalled event never admits an incomplete image. Microsoft documents the
authoritative value and the removed-device `UINT64_MAX` sentinel in
[GetCompletedValue](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-getcompletedvalue).
Event registration targets that exact pending value, following
[SetEventOnCompletion](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion).
The two-second deadline remains unchanged; waits use at most 20ms slices to check
completion despite a missed notification. Failed API calls or device removal
invalidate the surface. Present still precedes checked queue Signal; only a
successful Signal publishes an image target. New swapchain images start at zero
after the engine's existing queue drain. Reserving the removal sentinel is refused.

Bounded native records include image index, per-image target, global reservation,
completed values, wait result, elapsed time, queue/fence identities, and actual
Present HRESULT/flags. Four initial acquisitions and at most 28 further slow
successes are logged, plus at most four failures and 16 non-`S_OK` presentations.
These records enable a subsequent visible case to identify the actual stall.

The CPU checker canonically replays all four presentation patches from the pinned
commit and compiles their production helpers. Before canonical bootstrap, use
`--pending-bootstrap --out build/windows-x64/tools/d3d-image-completion-v3`;
the active cache must still match the exact preceding three-patch prefix. Its 106
assertions include completed-but-unset events, prematurely signalled events,
completion racing registration, different image targets, finite pending waits,
API failures and removal. This qualifies admission correctness; it does not yet
qualify visible frame pacing or prove the lost-event case caused the live failure.
