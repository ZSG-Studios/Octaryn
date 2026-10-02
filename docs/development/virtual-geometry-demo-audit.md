# VirtualGeometryDemo overnight reference audit

Read-only snapshot across 2026-09-30 23:47 to 2026-10-01 shortly after 00:00 EDT
(03:47-04:00+ UTC). Work was changing during inspection. This is reference
evidence for the [NVRHI handoff](nvrhi-renderer-handoff.md), not a finished-renderer
or Octaryn runtime acceptance report. No reference trees, binaries or processes
were changed; no builds/GPU runs were launched by this audit.

## Checked locations and identities

- Main: `C:/Users/Rose-X/Documents/VirtualGeometryDemo`, branch `master`,
  `d573773182a7f7b3ac2769b0c253c5f4800894d0` (licensing report). Only
  untracked `imgui.ini` was reported. No remote was listed by `git remote -v`.
- Worktrees: `C:/Users/Rose-X/Documents/VGD-worktrees`.
- Octaryn: `81c975257bdc865520f827c8cbbe80de0089d51c`, branch
  `codex/octaryn-world-library-renderer-plan`, with existing unrelated dirty
  host/content/module work preserved. `REQUESTS.md` was absent.
- Main reference submodules: Donut `69082638b014e3dae85a42c44b63e4187857c2c9`,
  NVRHI `ada8a144d37c31d67b31c40dc457a4bad4e083b7`, meshoptimizer
  `4c203430ca565cb59a468a91922c76c208169536`. These differ from Octaryn's
  locked dependency choices; do not silently change its registry pins.

Eleven VGD worktrees plus the main checkout were registered (12 entries total).
`falcor_fxaa` and `smoothing_cleanroom`
also contained source/specification artifacts but were not registered Git
worktrees at this snapshot. Some copied external submodule `.git` links were
broken; read-only `git status --ignore-submodules=all` exposed the main dirty
files without treating a submodule error as a missing project.

## What the overnight work actually contains

| Tree / observed head | Observed purpose/artifacts | Readiness limit |
| --- | --- | --- |
| `integration` / `0b96e01a684f0e186f77dc41082e730aba499617` | PRISM temporal/specular improvements, AA harness/metrics and accumulated integration work | Does not include every later branch change |
| `next` / `8e242b0226f9da406f31f6373f69ac0e0b4fa2ae` | FACET naming plus merged headless runner and in-process harness; Sponza/procedural DX12/Vulkan smoke images | Saved smoke captures do not qualify Octaryn or complete large scenes |
| `facet` / `5338f21ad3cb4a40bc36b6fac2cb17bfd0b2654c` | FACET rename/import naming work and focused regression artifacts | Refresh final merged identities; report identity can include dirty work |
| `harness` / `e63853a4135e6127726b4a3b83c0063e039352e1` | Camera paths, frame/depth capture, supersampled reference, quality metrics and scripts | Earlier report is for its historical binary/source state |
| `fastsuite` / `c37f6c0e473ffd7866d5bf40b758cd3927a44762` plus dirty edits | In-process benchmark matrix; MetricsPool/QualityEvaluator overlap CPU metrics/PNG work with GPU rendering; bounded buffers; CTest tiers | One quality job passed; full current regression/performance not established |
| `hang` / `8e242b0` plus dirty edits | Unattended modal-error suppression and proposed 60 s frame-stall watchdog/exit 8 | Running binary predates later source edits; hang fix not established |
| `leftovers` / `8e242b0` | Repeat-capture/LOD determinism comparisons under `out/det/lod_vk` | Recheck tolerances/source identity; no general determinism claim |
| `slang` / `8e242b0` plus staged/dirty edits | Twelve shader/include renames to `.slang`, edits to common declarations | CMake still globs HLSL and invokes DXC; actual Slang build/packaging unverified |
| `specular` / `7c72e66d97069ebb38c53f9e0ffe8fd0298267ce` | Geometric specular AA, normal-map filtering and filtered environment lighting | Analytic/environment shading does not provide Octaryn RT scene coverage |
| `temporal` / `9b8fd4ed37c4cab89bd7d3a55af2140df2b94b39` | PRISM reprojection, history clipping/disocclusion and stability work | Dynamic deformation, LOD changes and Octaryn FSR integration need new tests |
| `psmaa-robustness` / `2a6bb346f16033f179efadac6a5ca84b3e29d53e` | Spatial-AA robustness/self-test work | Integration and provenance must be reviewed independently |

The relevant earlier commits include `bef472a` (headless loop, no window/swapchain),
`ee5eb59` (multithreaded capture resolve/motion fields), `bb80822` (options/parser
split), `b147e85` (restartable harness/in-process jobs), then merge `8e242b0`.
The main checkout still predates that integrated overnight renderer.

The narrowly scoped Claude project folder found was the older
`C:/Users/Rose-X/.claude/projects/C--Users-Rose-X-Documents-NvrhiVirtualGeometry`.
Its available transcript tail was from the afternoon. It did not establish a
current overnight instruction or complete live task roster. The descriptions
above come from current source edits, commits, processes and scoped output logs;
they are observed work, not a verbatim Claude assignment/status feed.

## How the renderer works

At the main reference commit, `common/cluster_builder.cpp` and
`tools/vg_bake/vg_bake.cpp` cook meshoptimizer cluster-LOD geometry into
128-triangle clusters, a hierarchy and streamable pages. FACET-renamed trees
use `tools/facet_bake` and `common/facet_format.h` for the corresponding owners.
GPU traversal selects per-instance cuts and feeds mesh/compute indirect work.
Large clusters use mesh shaders; clusters with small projected bounds use
software rasterization. Alpha-masked and near-plane-clipped work stays on hardware.
Visibility IDs/depth drive deferred material reconstruction. Two-phase reversed-Z
HZB renders prior-history survivors and retests rejected work against current
depth. Background page I/O and GPU feedback drive a fixed pool/LRU policy.

Relevant main-commit references:

| Source | Responsibility / adaptation concern |
| --- | --- |
| `src/Renderer.cpp`, `shaders/cull.hlsl` | NVRHI pipelines/bindings, persistent traversal and fixed-count mesh-indirect recording; overflow/capacity handling needs qualification |
| `shaders/raster_hw*.hlsl`, `raster_sw*.hlsl`, `hzb.hlsl` | Hybrid visibility, alpha-mask discard and two-phase occlusion |
| `src/PageStreaming.cpp`, `common/page_streamer.h` | Page requests, locked parents and LRU; contains blocking idle waits and can increase requested pool capacity |
| `src/SceneResources.cpp`, `shaders/resolve.hlsl` | Bindless PBR, texture roles and reconstructed gradients; does not preserve all Octaryn attributes |
| `src/GpuTimers.cpp` | Per-section helper polls before reading and skips pending slots; useful for Octaryn's profiling-stall investigation |
| Overnight `src/PrismAA.*`, `PrismTemporal.*`, `shaders/prism_*` | Spatial AA, jitter/reprojection, depth rejection, clipping and specular stability experiments |
| Overnight `src/TestHarness.*`, `FrameCapture.*`, `CameraPath.*`, `BenchMatrix.*`, `tools/aa_metrics` | Repeatable quality sequences, references, motion fields and separate timing runs |

## Concrete incompatibilities to resolve

- **Compiler/platform:** main uses Donut/GLFW and ShaderMake/DXC HLSL. Octaryn
  retains SDL and independent Slang compilation/reflection plus registry pins.
- **Formats/memory:** reference pages are 128 KiB versus Octaryn's 64 KiB.
  `PageStreaming.cpp` raises too-small pools to locked pages plus 1024 pages.
  Fixed traversal/candidate buffers also consume substantial memory outside the
  page pool. A pool setting is not 512 MiB total-scene admission evidence.
- **Lifetime:** startup/upload/unload idle waits and fixed-age feedback mapping
  need Octaryn submission polling, cancellation and consumer-fenced reuse.
- **Atomics:** SM6.6/Int64ShaderOps plus a regular structured UAV table does not
  establish the required cross-vendor root-UAV guarantee. Retain backend extension
  and focused atomic winner tests; do not import the demo's 32-bit fallback.
- **Materials:** main explicitly treats glTF BLEND as MASK. Its importer reads
  NORMAL/TEXCOORD_0 and derives tangent frames; it does not preserve Octaryn's
  second UV, authored tangent/color or independent transparent pass.
- **Missing engine passes:** no demo-owned standard BLAS/TLAS ray scene was
  found. Tone-mapped RGBA8/analytic sky lighting does not replace HDR, RT shadows
  and reflections, FSR 2.2.1, RmlUi, items or live animated raster/ray geometry.
- **Organization:** main Renderer, baker and spatial-AA shader exceed Octaryn's
  500-line limit. Adapt focused responsibilities into established client/tool
  owners rather than copying oversized files or introducing demo runtime owners.

The reference notices identify unresolved smoothing and temporal-sampler
provenance. `smoothing_cleanroom/SPEC.md` exists and `falcor_fxaa` contains
Falcor's FXAA/notice material. At inspection, the clean-room directory held the
behavioral specification, not a qualified replacement implementation. Require
the specified stub/fresh-implementer/source-review process and final notices
before adopting that pass. This records project provenance findings, not a new
legal conclusion or an approval requirement for this documentation task.

## Inspected evidence and its limits

`next/out/smoke/Sponza_dx12.png` and `Sponza_vk.png` were visually inspected.
Both show the textured Sponza courtyard at 1280x720 with similar visible material
layout and no obvious gross scene loss in the static view. Their logs report
all 69 textures loaded and saved images. They are not motion, offscreen RT,
complete source-manifest or performance acceptance. The logged smoothed GPU
times are not a matched throughput distribution.

Older main `screenshots/procedural_vk.log` contains format/validation failures
despite an image being saved. Keep that failure distinct from the later smoke
artifacts; a screenshot alone cannot prove clean Vulkan validation.

The freshest inspected fastsuite receipt is
`fastsuite/out/fs/a8v/run1/results_Sponza_vk_quality.json`, started
`2026-10-01T03:59:40Z`, exit 0, reported zero warnings/errors. It identifies
headless Vulkan on RX 9070 XT, four metric workers, 160 metric evaluations,
7.18 s rendering completion and 30.57 s total wall time. Binary SHA-256:
`6a222a379edb97a7f73ec24bba11dde5d101b3823642e5934fa579fffa49978a`;
spec SHA-256:
`be540521078ca471f7bb841afba56c9e95c1308e914994112fcd954e80857dc7`.
Scope is five Sponza strafe 40-frame jobs: reference, control_blur, off, PRISM
and TAA-only. Four non-reference sequences account for the 160 metric evaluations.
It does not qualify steady rendering FPS, total scene memory, a complete current
test matrix, Slang artifacts, hang fixes, Bistro or Octaryn authority.

Other reports observed by the read-only audit report 27/27 FACET checks and an
older 53/53 harness suite. Their source states/workloads differ; preserve their
original identities and do not combine them into one finished-build result.
The latest observed hangfix Vulkan log stopped after device creation without
saved-frame/exit evidence. Rebuild-after-edit and guarded completion remain
required before accepting the proposed unattended watchdog changes.

The guarded per-section timer helper is a reference pattern, not proof that
all demo profiling is safe. `DemoApp.cpp` polls the whole-frame query but begins
it again even when the reused slot remains pending (main lines 343/361; `next`
lines 444/482 at inspection). Unguarded NVRHI time retrieval can wait. Require
pending-slot reuse, query/submission identity and device-loss/timeout tests;
fixed-age feedback readback mapping can block independently of timer queries.

## Refresh at final handoff

Record `git worktree list --porcelain`, branch/head/base, dirty and staged files,
submodule revisions, artifact hashes and report identities again. Inspect the
final Slang compiler commands and shader artifacts, final smoothing/source
notices, query/watchdog behavior and integrated DX12/Vulkan quality reports.
Then apply the owner map and dependency gates in the
[handoff plan](nvrhi-renderer-handoff.md). This snapshot is not a live monitor.
