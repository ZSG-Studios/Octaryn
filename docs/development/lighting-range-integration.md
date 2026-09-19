# Lighting range, visibility and controls — 2026-09-18

The user accepted the DDGI stability repair and requested farther GI coverage,
better performance, consistent shadows/reflections, leak prevention and current
menu controls. Per-observation history and explicit change notification remain
the baseline. Full-detail raster geometry and standalone Slang RHI are required.

## Baseline measurements

Evidence root: `logs/client/lighting-range/`.

- `before/visual-dx12-0-native-a4pqyblf`: 2560x1440 Native AA, moving natural-world
  qualification, original fine16/coarse128 settings. Median frame wall 4.335 ms,
  p95 9.961 ms, maximum 83.018 ms in the measured window excluding captures.
  DDGI GPU p95 0.373 ms; batching makes per-rendered-frame medians misleading.
- `before-256/torch-fine16-coarse256-k529jmv4`: explicit 60 FPS setting, fine16,
  coarse256, production authoritative torch sequence. 32,768 fine and 49,152
  coarse probes, 140,967,936 reported bytes; trace/update medians 0.151/0.094 ms.
  Dirty backlog drains. A larger configured radius is not proof of occlusion
  outside the loaded ray scene.

## Confirmed integration gaps

- The persisted far-GI control was absent from the live menu. It is now exposed
  separately from near-GI, with block units and cost/loaded-scene explanations.
- Live ray/range changes did not synchronize staged settings; a later Apply
  could restore stale values. The UI now updates both representations.
- BLAS publication compared opaque/lava face counts to decide whether occlusion
  changed. Equal counts do not establish equal geometry. Publication now skips
  occlusion invalidation only when both old/new meshes contain no blockers.
  A moved-roof GPU case explicitly preserves face counts and checks lighting.
- The selector now controls actual GI update throughput, with independent
  shadow/reflection distance controls. Low no longer disables RT only at startup.

## Implemented range and performance behavior

Coarse radii 128/256/512/1024 now use a bounded 32x12x32 grid with spacing
8/16/32/64 blocks at ordinary configuration. Coarse spatial lighting detail is
traded for reach; near GI and full-detail voxel meshes are unchanged. Changing
one GI volume retains the other's resources/history. Unchanged camera cells
skip redundant full-grid scrolling walks while continuous fades still update.
See [native range evidence](ddgi-range-native.md).

The four **GI performance** levels allocate 0.12/0.22/0.35/0.50 ms per enabled
volume per 60-Hz tick. They control bounded adaptive throughput, not an FPS
guarantee, shadow quality or reflection roughness. Live changes preserve history.
Menu ranges, zero behavior, costs, resident-scene dependency and staging are
documented by the actual controls.

## Visibility and reflected receivers

Blocked-probe weights no longer normalize negligible visibility back to full
exterior brightness. Dark coverage remains dark rather than falling through to
coarse GI or sky. Full near coverage skips the unused coarse gather. Probe miss
distances describe the actually traced segment. Reflected diffuse surfaces use
the same visibility-tested uncovered sky policy as primary surfaces, respect
metallic masking, and no longer fade known geometry into sky near the cutoff.
See [shader reference and limitations](ddgi-leak-support.md).

## Final executed evidence

- Native client bundle and world-mesh GPU fixture build successfully:
  `logs/build/lighting-range-{bundle,probe}.log`.
- All 77 shader cases pass on DXIL, SPIR-V and Metal source.
- GPU blocked-probe fixtures pass **120 cases on each of Windows DX12/Vulkan**:
  `leak/{d3d12,vulkan}.log`. These execute production sampling on authored GPU
  buffers; scene traversal is qualified separately.
- Production dynamic suites pass **24 transitions per backend**:
  `dynamic-dx12/` and `dynamic-vulkan/`. The equal-count moved roof suppresses
  sunlight and refreshes correctly. Measured sustained 80% response ranges are
  0.029–0.652 seconds DX12 and at most 0.817 seconds Vulkan.
- Production UI: **852 general, 59 FSR, 1,723 inventory checks** pass at four
  viewports; no OS events injected. Settings probe passes **167 checks**.
- Native range tests cover every coarse radius 1–1024, fine radii 1–32, bounded
  allocations, retained independent histories and live performance tiers.

### Before/after Windows DX12 measurements

| Workload/measurement | Before | After |
| --- | ---: | ---: |
| Fine16/coarse256 total DDGI buffers | 140,967,936 bytes | 98,353,152 bytes |
| Coarse256 probes | 49,152 | 12,288 |
| Coarse256 torch trace median | 0.151 ms | 0.181 ms |
| Coarse256 torch update median | 0.094 ms | 0.058 ms |
| Coarse256 torch composition median | 0.246 ms | 0.126 ms |
| 1440p moving natural-world median frame wall | 4.335 ms | 3.729 ms |
| Same natural-world p95 frame wall | 9.961 ms | 8.642 ms |

These are paired explicit hidden qualification runs, not ordinary VSync FPS or
universal speedups. Trace cost increases with longer segments while update and
composition cost decrease. Natural-world medians improve approximately 14%.
Capture frames are excluded from frame-wall statistics. Large CPU/streaming
outliers remain; this is not a hitch-free startup claim.

Final natural sequence `after/visual-dx12-0-native-nk_p6hbf` and authored pool
before/after sequences were inspected. Close pool sequences on both backends
show actual reflected columns/walls and retain temporal history:
`pool-close/visual-dx12-0-native-u13rcfzh` and
`pool-close/visual-vulkan-0-native-go00r97g`. Their measured median frame wall
times are 2.449 ms and 2.707 ms respectively at 1920x1080; these close views have
no paired old-build performance baseline. Their maxima include substantial
outliers and must not be represented as stable instantaneous frame rates.

### Maximum-range scope

`after-1024/torch-fine16-coarse1024-y5tpx5ot` executes requested radius 1024 with
12,288 coarse probes and the same 98,353,152 total bytes. All 24 tracked near
histories refresh in all phases. This proves bounded allocation and operation
at that setting, **not visibility correctness 1024 blocks away**: this fixture
loads radius-4 world geometry. Coarse boundaries also fade/interpolate.

## Remaining integration limits

Moment-based visibility cannot prove every thin-wall case, and rays cannot
occlude against missing/unloaded geometry. The conservative equal-count fix
also refreshes mixed columns whose transparent edits rebuild otherwise unchanged
opaque geometry; exact occluder signatures could reduce that work later.
Water reflections remain the original single-ray model: filtered roughness
lobes and reflected local direct-light evaluation remain unimplemented. Range
misses still use sky, so cutoff policy needs further work. These results do not
justify an absolute no-leak or complete AAA/PBR-parity claim.

Windows DX12/Vulkan are runtime-qualified here. Emitted Metal does not establish
macOS execution; Linux hardware runtime was not exercised in this task.
