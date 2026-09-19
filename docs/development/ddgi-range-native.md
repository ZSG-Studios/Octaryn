# Bounded DDGI range and native scheduling — 2026-09-18

This records the native part of the farther-GI/performance repair. Actual GPU
qualification is serialized by the parent task; CPU results here do not establish
far-field occlusion or visual quality.

## Reference and diagnosis

Inspected the actual pre-SRC `d554b64` `DDGISystem.cpp` and the restored current
volume owners before changing them. That pre-SRC snapshot has the fine-only
reconfiguration; `DDGIVolumeConfig.h` does not exist at that revision. The current
restored independent coarse/fine configuration and subsequent stability repairs
are the active baseline.

Also inspected NVIDIA RTXGI-DDGI revision
`f33e496ca31b3f0eec1c4e2cbaa8bb620e337fa6`,
`rtxgi-sdk/include/rtxgi/ddgi/DDGIVolume.h`: counts, spacing and maximum ray distance
are independent properties; allocation is driven by counts and storage/ray formats.
The established per-observation hysteresis contract in `ddgi-stability.md` remains.
No reference implementation code was copied.

The slider/backend clamp was already 1024 blocks. The problem was that larger
ranges expanded to 64×12×64 probes, while maximum trace distance remained only
96 / 192 / 384 blocks at coarse radii 256 / 512 / 1024. The default 128 setting
means a **nominal** half-width of 128: 32 probes times 8-block spacing, divided by
two. It is not a guarantee of full-weight interpolation exactly at the boundary.
The shader inspected initially also had an asymmetric fade and fewer interpolation
cells than the nominal extent; shader coverage fixes are owned separately.

## Native changes and limits

| Requested coarse radius | Grid | Spacing | Maximum trace distance | Buffer bytes |
| ---: | --- | ---: | ---: | ---: |
| 128 | 32×12×32 | 8 | 256 | 37,339,136 |
| 256 | 32×12×32 | 16 | 512 | 37,339,136 |
| 512 | 32×12×32 | 32 | 1024 | 37,339,136 |
| 1024 | 32×12×32 | 64 | 2048 | 37,339,136 |

These bytes include controls, probe state, irradiance, distance moments,
variability, ray results and both selection/history-interval buffers, at the
ordinary 176 rays, 6×6 irradiance and 8×8 distance settings. They exclude shared
world acceleration structures, pipelines, query pools and debug-only resources.
The coarse bound is **35.609375 MiB**, formerly **76.25 MiB** at radii 256–1024:
a 53.3% buffer reduction. At maximum environment overrides (512 rays and 16×16
irradiance/distance), the bound is 143,507,456 bytes / 136.859375 MiB.

Fine radius remains 0–32, with one-block spacing and the same counts, distance
and history behavior. Fine-16 remains 32,768 probes / 61,014,016 bytes. Combined
coarse-256/fine-16 is therefore **98,353,152 bytes**, versus 140,967,936 before.

Larger spacing reduces the coarse field's spatial resolution and small-feature
lighting detail; it does not change mesh detail or ray/voxel intersection steps.
No voxel LOD is introduced. The hardware scene contains resident geometry only:
render distance 4 supplies roughly a 128-block resident radius, so a 1024-block
probe envelope cannot establish occlusion from unloaded terrain.

Reconfiguration now only reallocates the volume whose configuration changed.
Changing far range retains fine resources, accumulated observations, timestamps,
and explicit refresh markers. Fine-range changes likewise retain coarse history.
Repeated identical settings do not reallocate. Disabled volumes remain independent.

Scrolling updates the continuous fade origin each call, but skips the grid walk
when the integer origin is unchanged. This also removes the duplicate whole-grid
walk from the prepare/schedule sequence. Crossing a cell boundary still republishes
the exposed plane and preserves every overlapping probe's history.

## Runtime GI performance levels

The former lighting-quality selection did not affect DDGI workload. Low instead
disabled RT through the scene setter, and changed only a startup raster-shadow
resolution. The selection now controls **GI scheduling performance**, independently
of RT enablement and the separate shadow/reflection range controls:

| Level | GPU allowance per volume per 1/60 second |
| --- | ---: |
| Low (0) | 0.12 ms |
| Medium (1) | 0.22 ms |
| High (2) | 0.35 ms |
| Ultra (3) | 0.50 ms |

These are timestamp-feedback scheduling targets, not guaranteed per-frame GPU
ceilings. Two active volumes each receive that allowance. Lower levels trade
refresh throughput/response speed, not probe spacing, ray count, meshing detail,
or history retention. Quality changes scale the live budget/credit/debt, retain
observations, and allocate no resources. High retains the former 0.35-ms target.
The old quality-driven RT gate and Low-only raster-resolution override are removed.
Shadow/reflection workload is not tier-mapped by this change.

## CPU evidence

Run `python tools/validation/validate_ddgi_range.py`.
`logs/tools/ddgi-range-after.log` records:

- Every integer coarse radius 1–1024 and fine radius 1–32; unsigned overflow and
  invalid spacing inputs; exact default/maximum buffer bounds.
- Continuous subcell fade and retained history; exactly one exposed probe plane
  after a one-cell scroll.
- 324 near-volume receiver positions across four far radii and positive/negative
  camera poses, including ±32 blocks vertically: every required interpolation
  corner exists in the actual CPU control grid. This is grid coverage, not proof
  that GPU probes are unoccluded or have valid irradiance.
- Extracted production reconfiguration and quality setters with only GPU resource
  creation/host discovery mocked: independent allocation, preserved histories,
  disable/re-enable, all four live levels and 1,000 repeated setter calls per level.
- Production scheduler at 30/60/144/600 FPS and all four levels. Over the measured
  three-second window, modeled work is exactly 21.6 / 39.6 / 63 / 90 ms respectively,
  matching each level's allowance. These are CPU-modeled costs, not GPU measurements.

`python tools/validation/validate_ddgi_light_publication.py` also passes the actual
source-publication hook, its old-behavior negative control, all 35 scheduler cases,
and the extended response/fairness matrix after these changes.

The before-source snapshot is retained under
`build/release-windows/tools/ddgi-range/before`. Three alternating runs of the same
CPU workload (2,000 calls per case) are in
`logs/tools/ddgi-range-comparison{.json,-raw.log}`. Median CPU prepare-scroll plus
schedule time, microseconds per call:

| Volume | Before | After |
| --- | ---: | ---: |
| Coarse 128 | 704.523 | 470.426 |
| Coarse 256 | 2286.861 | 281.261 |
| Coarse 512 | 2513.043 | 259.490 |
| Coarse 1024 | 2537.304 | 265.631 |
| Fine 16 | 1849.394 | 691.780 |

The isolated unchanged-cell scroll drops from hundreds of microseconds to the
constant-time origin check. These CPU timings include workstation contention and
are not frame-time/GPU speedup claims. The repeated comparison preceded the live
tier addition; its High target and scheduling selection are unchanged.

## Serial GPU handoff

Use the parent task's rebuilt bundle, matching the baseline's High quality and
60-FPS case. For example, replacing `<bundle>` with that bundle path:

```powershell
python tools/validation/qualify_torch_response.py --client-bundle-root <bundle> --evidence-root logs/client/lighting-range/after-256 --backend dx12 --fine-radius 16 --coarse-radius 256 --frame-cap 60 --timing-profile
python tools/validation/qualify_torch_response.py --client-bundle-root <bundle> --evidence-root logs/client/lighting-range/after-1024 --backend dx12 --fine-radius 16 --coarse-radius 1024 --frame-cap 60 --timing-profile
```

Repeat on Windows Vulkan and inspect actual images, probe validity, pending/age
and trace/update timings. Compare the same saved natural view separately.
Radius-1024 allocation in a radius-4 world establishes bounded allocation only;
farther occlusion needs resident source geometry at the tested receiver/ray locations.
