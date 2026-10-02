# NVRHI renderer handoff and AAA qualification

Status: integration plan prepared from the overnight 2026-09-30 / 2026-10-01
reference audit. Implementation and runtime acceptance remain open.
The [renderer rewrite](nvrhi-renderer-rewrite.md) owns the completion checklist;
the [reference audit](virtual-geometry-demo-audit.md) records what was inspected.

## Completion of the reference renderer is the handoff trigger

When Claude's FACET / PRISM work is ready, inspect the final integrated source
and evidence again before choosing an import commit. The main
`VirtualGeometryDemo` checkout is behind the overnight worktrees. Neither a
branch name, a completion message nor the old main checkout identifies the
finished renderer. Record a single integrated commit and every remaining dirty
file; require reproducible binaries and shaders from that exact state.

Review these deliverables before adapting reference code:

- Exact base/head commits, dependency revisions, changed-file inventory,
  source-to-destination map and notices for each adopted responsibility.
- Real Slang compilation to both DXIL and SPIR-V, entry-point profiles, binding
  reflection and packaged artifact hashes. Renamed extensions are insufficient.
- DX12 and Vulkan validation reports with exit codes, scene and shader hashes,
  device/driver identity and inspected images from the same binary.
- Headless harness completion, bounded query polling, unattended failure exit,
  timeout cleanup and deterministic camera/capture identities.
- PRISM spatial/temporal, specular filtering and smoothing provenance reviewed
  separately, with motion/disocclusion images and matched quality metrics.
- A resolved integration graph: identify which specular, temporal, robustness,
  FACET, harness, fastsuite, hang, leftovers and Slang changes are actually merged.

Reference review is read-only. Do not reset, merge, rebuild or terminate Claude's
active trees/processes while taking the handoff snapshot. Copy only reviewed
responsibilities into Octaryn's owners; do not substitute the demo application.

## Architecture to preserve

Octaryn already contains `SceneGeometryPool`, `SelectionResources`,
`SceneRayScheduler`, batched `SceneRasterWorld` and checkpointed
`SceneHierarchyPreparation`. Preserve these foundations and port their GPU
contracts. Their existence is separate from complete large-scene qualification.
The earlier allocation-problem discussion in
[scene geometry scaling](scene-geometry-scaling.md) is historical context.

The intended production frame is:

1. Retain selected-content CPU snapshots and admit one complete raster/ray cut.
2. Decode/upload bounded page work; publish only submitted, generation-valid data.
3. Select instances and hierarchy groups; generate global hybrid-raster work.
4. Produce visibility, reconstruct authored materials and compose BLEND separately.
5. Advance bounded ray builds; publish a complete immutable BLAS/TLAS snapshot.
6. Execute required RT lighting, HDR/sky/clouds and temporal/FSR presentation.
7. Render module-declared UI/items and retire resources after all consumers finish.

Submission order and barriers must satisfy the actual pass dependencies. A new
ray snapshot becomes visible only when its uploads/builds/copies are complete;
lighting may retain the previous complete snapshot while preparation continues.
Temporal history commits only for successfully submitted frames.

## Reference adoption decisions

| Reference work | Octaryn decision | Evidence needed before promotion |
| --- | --- | --- |
| NVRHI command lists, explicit bindings, indirect mesh/compute work | Adapt recording patterns to existing owners and independent Slang artifacts | Binding/offset fixtures, root-UAV atomics and backend validation |
| Persistent GPU traversal | Candidate optimization after existing selection is ported | Capacity/iteration overflow tests, complete-cut parity, per-instance error and measured benefit |
| Two-phase HZB | Candidate repair for moving-camera occlusion | Pan/strafe/cut/disocclusion captures; conservative retest with no holes or blink loop |
| Page streaming | Retain Octaryn identity/generation/fence policy | Delayed consumers, missing children, eviction/reuse and aggregate admission |
| FACET formats, quantization and 128-KiB pages | Keep Octaryn's versioned 64-KiB pages and cook/source seals | Any later format change needs explicit migration, boundary and attribute/error qualification |
| Bindless PBR and specular filtering | Adapt binding ideas; evaluate filtering separately from swap parity | Both UV sets, authored tangent/color/normal fidelity, mirrored/sheared instances and motion quality |
| PRISM spatial/temporal AA | Optional quality candidate after existing temporal/FSR parity | Cleared provenance, Slang artifacts, moving materials/LOD/dynamic geometry, reactive/history validity |
| Headless harness, metrics and nonblocking timer polling | Adapt focused validation/tool responsibilities | Correct frame/query identity, bounded waits, device-loss/stall exit and independent quality/timing runs |
| Donut/GLFW/DXC application and demo UI | Keep Octaryn SDL, host lifecycle, Slang and RmlUi | Existing product surfaces and module contracts preserved |

The demo's BLEND-as-MASK behavior cannot enter production. Its analytic sky
reflection is not the required ray reflection path. Its pool-growth heuristic,
blocking idle waits and raw background I/O thread do not replace Octaryn's
aggregate budget, submission timeline or host scheduler policy.

## Implementation order and reviewable exits

Each slice records changed owners, exact artifact identities, commands, exit codes,
captures, measured limits and remaining failures in the main checklist. Serialize
native builds and GPU runs. Keep every touched source/code file below 500 lines.

| Slice | Work and owners | Required exit before the next dependent slice |
| --- | --- | --- |
| 0. Freeze controls and reference | Coordinator; build/tools; selected source packages | Reproducible current-renderer build, Bistro images/timing controls; frozen final FACET commit, dependency and provenance inventory |
| 1. Shader and lifetime contracts | Shader compiler/cache owners; material ABI; frame owners | Independent Slang artifacts/reflection; 1072-byte map material and 160-byte ray instance records; descriptor defaults/ranges; submission-token and abandoned-frame contracts |
| 2. NVRHI bootstrap/extensions | Focused DX12/Vulkan bootstrap; registry/build helpers | Registry pin/patch receipt; validation; root-UAV int64 winners; allocation-free AS sizing; aligned caller scratch; asynchronous compaction; physical allocation telemetry |
| 3. Shared residency and selection | SceneGeometryPool, GeometryStream, PageResidency, SelectionResources/SelectionGpu | Global keys/generations and tagged feedback; upload/consumer retirement; packed roots; cancellation; bounded memory under delayed consumers |
| 4. Global raster/materials | SceneRasterTables/SceneRasterWorld, WorldGeometryRaster, material shaders | Hybrid visibility/resolve parity; parent-or-all-children coverage; original instances; OPAQUE/MASK/BLEND and authored attribute captures |
| 5. Complete ray publication | SceneRayScheduler, RayGeometry, WorldGeometryRay, WorldRaySnapshot | Whole-cut admission; standard cross-vendor BLAS/TLAS; material offsets; offscreen coverage; bounded scratch/compaction and retained snapshots |
| 6. Presentation/dynamic content | HDR, sky/clouds, Hi-Z, temporal/FSR, UI/items/Animation | FSR 2.2.1 preserved; resize/minimize/history behavior; RmlUi clipping/layers/filters; current/previous deformation and live raster/ray dynamic-content integration |
| 7. Selected-world integration | SceneAssets/SceneSession/publication; host.scene adapter; local session | Metadata-only menus; shader/pipeline progress; first GPU publication; cancel/unload/switch/save-reopen; exact Box3D collision and stable authoritative pose |
| 8. Cutover and removal | Coordinator; CMake/dependencies/validators/package | octaryn_all; active NVRHI execution and closure audit; no active slang-rhi/ShaderCursor/IShaderObject or old patch/bootstrap path; inspected DX12 runtime |
| 9. Independent qualification | Validation owners; Vulkan then native Linux | Independent backend/runtime evidence, complete large-source world and memory/authority/soak gates; AAA workload measured separately |

Extract independent Slang SDK acquisition before removing slang-rhi tooling.
Use fixed-count indirect mesh dispatch initially. DX12 count-buffer mesh dispatch
remains unsupported at the locked NVRHI pin; preserve explicit unsupported tests
and do not emulate it through CPU readback.
Keep the current renderer as the pre-cutover control until replacement parity is
reviewed; the final package has one active backend interface. Deferred macOS/Metal
and optional NVIDIA-specific paths cannot block or substitute DX12/Vulkan gates.
Lighting algorithm redesign remains a separate approved scope after swap parity.

`ScenePreparationCollision*`, shared `CharacterMotion` and server map-world
owners retain exact source collision and Box3D movement independently of visual
LOD. Renderer admission never makes the client authoritative.

NVRHI requires application-owned native device/queue bootstrap. Its internal
resource references and upload/scratch working sets also affect retirement and
memory reporting; call garbage collection during frames/loading/unload and
measure retained command-list allocations. See the pinned
[programming guide](https://github.com/NVIDIA-RTX/NVRHI/blob/d0c8e30d5f8d58c3b838b06aa1d8d0a912bea076/doc/ProgrammingGuide.md).

## AAA acceptance matrix

| Gate | Required workload/evidence | Completion condition |
| --- | --- | --- |
| Backend/build | octaryn_all, packaged client/server, configured link/dependency closure, native API + NVRHI validation | Real NVRHI execution; retained Slang tooling; no removed backend linkage or active pass bypass |
| Content completeness | Complete imported source manifest, original node/instance/material IDs, source/cook/order/hierarchy seals | No omitted/duplicated source domains; complete parent or complete children; bounded metadata residency |
| Visual quality | Bistro and material stress fixtures; opaque/masked/transparent, authored/POSITION-only, both UVs, tangent/color, nonzero offsets, mirrors/shear | Inspected output and G-buffer/material images; no cracks, holes, lost transparency, incorrect normals or texture roles |
| Motion/temporal | Static, pan, strafe, authoritative travel, cuts/teleports, disocclusion, stream/LOD changes, moving items/characters | No persistent shimmer, occlusion blinking, invalid history, geometry trails or frozen animation; report quality metrics with images |
| Ray correctness | Offscreen colored/masked reflectors, multi-material/instance cuts, scene replacement and dynamic geometry | Complete admitted ray coverage during publication; correct shadows/reflections and material orientation; honest requested/achieved error |
| Scene memory | Default 512 MiB admission, pressure/missing children, active/pending/compacting/retired generations | Physical scene-owned capacity charged once and bounded; rejected refinement retains prior complete cut; no automatic budget increase |
| Lifecycle/authority | Cold/warm selected open, root coverage, steady state, boundaries/collision holds, resize/minimize, cancel/unload/switch, independent save/reopen | Exit 0 and clean owned-process retirement; stable server pose/exact collision; listen/connect checks when networking behavior changes |
| Large world | Source-complete hierarchy and full-world runtime with original instances/materials and offscreen rays | Actual guarded open/travel/captures under defaults; a small fixture or completed cook alone cannot pass |
| AAA performance | Native 3840x2160, Bistro, 1,000 items, 128 independent animated 100k-triangle characters, 64 joints and eight morph targets, required RT/material quality | 240 genuinely rendered FPS; frame mean and p99 <=4.17 ms, CPU/GPU costs reported; workload and resolution identities retained |
| Platform | DX12 Windows, Vulkan Windows, native Vulkan Linux | Separate current build/runtime/capture reports; one API or emitted SPIR-V does not qualify another platform |

Report scene reservations, logical device bytes, actual committed heap capacity,
AS result/scratch, upload/readback, retired bytes, process GPU usage, RSS and
compressed cache size separately. The 512 MiB scene budget is not a claim that
the entire process, frame targets or OS/driver memory fits that envelope.
Define charged domains before recording acceptance; do not move existing scene
costs out of the ledger to obtain a pass.

Measure preparation, cold/warm open, shader/pipeline warmup, first complete raster
and ray coverage, settled rendering, moving residency, boundary crossing and
retirement/shutdown separately. Use at least three matched repetitions and
retain failures and run spread. Quality captures/validation/counters and clean
timing runs have separate receipts. Generated frames never count as engine FPS.

Runtime checks stay hidden, finitely frame-capped, watchdog-supervised and free
of input injection/focus changes. Keep the 50 ms sustained-frame and 2 s heartbeat
guards. The current capped capture cannot prove maximum 240-FPS throughput:
add a finite, explicitly recorded high-cap timing mode before that gate. Do not
silently use the current `--uncapped-fps` option. Use an outer process watchdog
for query/device waits that could prevent the in-frame heartbeat from running.

## Existing command entry points

These command shapes were checked against current source. They were not executed
in this planning pass and must be rechecked after the backend/tooling cutover.
Run from the repository root; outputs stay in the existing owner layout.

```powershell
python tools/build/windows.py --action configure --preset release-windows
python tools/build/windows.py --action build --preset release-windows --target octaryn_all

python tools/validation/run_map_gpu_probe.py --backend d3d12
python tools/validation/run_map_gpu_probe.py --backend vulkan

python tools/validation/capture_map_world.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/nvrhi-handoff/dx12-quality --backend dx12 --ray-tracing on --width 3840 --height 2160 --upscaler-mode 0 --render-scale 1 --frames 360 --capture-min-frame 300 --captures 3 --stride 16 --max-frame-ms 50 --rhi-validation
```

Repeat with `--backend vulkan` and its own evidence root. Native temporal AA
(`--upscaler-mode 1`) and FSR modes need separate matched cases. Use
`--camera-motion` for the camera fixture and `--gameplay-route <route.json>` for
actual authority movement; a camera-only route cannot qualify player collision.
Add resize/minimize/cancel/device-loss cases through code fixtures, not UI input.

Port `validate_render_pipeline.py` before using it as a NVRHI gate: it currently
requires slang-rhi linkage and bans all first-party raw Vulkan. Replace that
closure requirement, permit narrowly identified bootstrap/extension owners,
and continue rejecting scene/pass-level native bypass and non-Slang shaders.
Port the GPU probes, validation-error parsing and timer/submission identities too.
Adapt the demo's guarded per-section polling helper, then test whole-frame query
slots independently: the observed DemoApp can restart a still-pending query.
Never read or reuse a query merely because it is several frames old.

`capture_scene_stream.py` has separate prepare/run/inspect modes. Its focused
fixture covers publication, retained consumers and authority; it does not cover
the large-world or animated AAA workload. Redefine historical performance-matrix
variants around the sole active VG path instead of reviving old opaque modes.

## Handoff status to record next

- [ ] Freeze Claude's final integrated renderer commit and evidence inventory.
- [ ] Verify actual Slang build/packaging on DX12 and Vulkan.
- [ ] Resolve/review PRISM smoothing and temporal-sampling provenance.
- [ ] Verify new unattended/watchdog fixes using a binary built after the edits.
- [ ] Port nonblocking query handling; qualify ordinary profiling without stalls.
- [ ] Produce current Octaryn baseline controls and start slice 1.

None of these implementation/acceptance gates was passed by this documentation
review. Reinspect the completed reference renderer at the handoff; this plan
does not schedule a background monitor or alter Claude's ongoing work.
