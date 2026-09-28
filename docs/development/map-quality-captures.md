# Matched ray-tracing quality captures

`tools/validation/capture_map_quality.py` runs isolated hidden map captures through
the existing watchdog. It never moves the authoritative actor or injects input.
Run `--dry-run` first to write the commands and geometry-based candidate views.
No GPU process starts in that mode.

The default sequence uses eight native 2560×1440 images for each of three views:
foliage, shop glass, and Bistro signs/metal. These produce 24 images per rendering
path. Reference and adaptive paths run sequentially; `--include-sparse` adds sparse
sampling. `--dense` changes each view to 64 images at 640×360, with a three-frame
stride. Every launch stops after 400 authoritative-ready frames. Separate DX12
and Vulkan invocations are required.

The native sequence samples ready frames 180 through 348 in increments of 24.
This includes translation/rotation, settling, the cut at 324, and post-cut recovery.
The dense sequence samples every third frame through 369. Neither sequence is an
unbroken video. Generated `review.html` offers synchronized stepped playback and
a frame slider; its playback rate is a review aid rather than real frame timing.

`map_quality_views.py` reads only GLB JSON and records the named materials' baked
world-space bounds. The foliage candidate targets alpha-tested leaves and hedges;
the glass candidate targets `MASTER_Glass_Exterior`; the sign candidate targets
`Bistro_Sign_Main` and `Banner_Metal`. Optional `--views chrome_interior` targets
`Metal_Chrome1`. Actual material visibility, roughness and reflection detail must
be confirmed from images: names, bounds and camera directions alone are insufficient.
The known street origin has existing capture evidence, but the additional views
remain candidates until inspected for walls, occlusion and useful framing.

The suite rejects sequences that lack captured motion/settle/cut/post-cut phases,
whose first captured camera differs from the requested origin, or whose sky state
changes. Compared paths must match actual captured camera poses, ready frames,
lighting, map hash and output dimensions. Each child capture also retains its
normal executable/shader/asset identity records. The suite leaves visual acceptance
pending; matching poses and a clean runtime do not prove image quality.

Every captured frame also records the observed FSR jitter, temporal reset state,
global renderer frame and shadow sampling frame. Comparison reports jitter-phase
equality separately: startup submission counts can shift the global-frame jitter
sequence between launches even when ready-frame cameras match. A mismatch limits
pixel-difference and temporal-noise conclusions but does not discard the captures
or invalidate three-run aggregate performance results. New observation records
also include the actual reflection phase; older binaries leave it unknown.

Explicit `--fixed-sampling` aligns FSR jitter and shadow/reflection sample frames
to the authoritative-ready frame index, and supplies a 16.6667-ms presentation
delta. Activation requires a hidden, bounded, locked-camera fixture. The suite
checks the captured phase metadata against its camera CSV. Real wall-clock and
GPU timing logs remain untouched, but `timing_qualification` is false: this mode
is exclusively for visual comparisons. Ordinary rendering without the flag keeps
its existing frame indices and elapsed presentation timing.
Equal phase indices do not imply identical ray directions across different
sampling policies; they remove the unrelated startup-frame offset.

`--rt-history-search` enables the experimental 2×2 reflection history search on
adaptive paths only. It remains off by default and leaves shadows and reference sampling
unchanged. Each candidate must pass the existing normal, depth, material and
view-direction checks; the nearest valid sample wins. No disocclusion threshold
is relaxed. The separate
reduced-resolution reflection correction reconstructs from the actual sampled
source-pixel center and raster aspect ratio, including odd render dimensions.
`validate_reflection_reprojection.py` checks the CPU equations and selection
boundaries; compiled shaders and actual GPU image/counter evidence remain required.

The shadow 2×2 search experiment was rejected after the build13 opt-in DX12 run
hit a GPU fence timeout (`logs/client/build13-isolation/search-on/map-dx12-on-0-gz2n8ir7`).
The paired default/reference run passed. The experiment and its shader variants
were removed, restoring the previously passing shadow shader bodies. These runs
isolate the failing feature; they do not establish a compiler or driver root cause.

Inspect foliage silhouettes, contact shadows, glass, sharp reflections, rough
reflection noise, newly exposed surfaces, and any persistent ghosts or light leaks.
Capture overhead makes these runs unsuitable for FPS acceptance. Moving-item
rendering is outside this suite: the client currently lacks an item-pose visual
contract. Authority item tests cannot substitute for that missing visual coverage.
