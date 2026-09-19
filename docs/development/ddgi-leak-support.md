# DDGI visibility support and reflected receivers — 2026-09-18

## Inspected references

Before editing, inspected the actual `DDGIGetVolumeIrradiance` implementation in
NVIDIA RTXGI-DDGI `f33e496ca31b3f0eec1c4e2cbaa8bb620e337fa6`,
`rtxgi-sdk/shaders/ddgi/Irradiance.hlsl`, and Octaryn's pre-SRC `d554b64`
`DDGISample.slang` / `WorldFluidShade.slang` through `git show`.

RTXGI deliberately retains a 0.05 visibility floor to provide a fallback when
every probe is blocked. That policy is unsuitable as a visibility guarantee:
normalizing those weights restores full exterior radiance inside an entirely
occluded cage. This change is an explicit conservative deviation, not a claim
that the reference implements hard visibility or that moment-based DDGI can
prove visibility through arbitrary geometry.

## Changes

- `DDGISample.slang` removes the visibility floor and retains the largest
  Chebyshev support as an absolute multiplier after normalized interpolation.
  A cage with one fully visible corner still preserves constant irradiance;
  an entirely blocked cage cannot normalize tiny support to full brightness.
  Valid occluded observations retain coverage, so rejection does not invoke a
  bright coarse field or the sky fallback. Pending observations retain the
  existing handoff behavior. No extra visibility rays are added to this gather.
- The denser volume is sampled first. Full dense coverage returns immediately,
  avoiding the unused sparse gather. This reduces source-level sampling work;
  GPU speedup has not been measured.
- `DDGITrace.slang` records the actual ray query's capped segment length on a
  miss. Previously it recorded the requested DDGI range even when the shared
  tracer had clamped it to a shorter `raySettings.y`, falsely advertising open
  space beyond the tested segment. Environment classification on misses still
  has the residency/range limitation described below.
- `DDGIResolve.slang` shares the visibility-tested uncovered sky fallback
  between HDR composite and reflected diffuse surfaces. Reflections no longer
  use unrestricted ambient in an uncovered, enclosed surface while GI is on.
  The fallback retains the original both-GI-volumes-disabled behavior.
- Reflected diffuse shading respects the hit material's metallic mask and keeps
  emission outside that mask, matching the diffuse convention of tracing and
  composite. A known reflected hit is no longer faded into unoccluded sky in
  the last eighth of reflection range.

The sky fallback already uses eight fixed directions for a camera receiver;
reflected hits now use that same bounded quadrature only when uncovered. This
can add up to eight sky transmissions per uncovered reflected hit. Covered
hits add none. Its real fluid-heavy GPU cost and appearance need measurement.
There is no eight-ray per-pixel probe-cage visibility test.

## Audit findings and boundaries

- Sun direction/bias visibility is already shared by probe-hit direct sunlight,
  reflected-hit sunlight, and fluid glints through `world_ray_visibility`.
  Reflected/probe receivers cannot reuse camera-pixel shadow history: the
  world-space receiver differs. Primary composition uses the resolved temporal
  sun visibility once, rather than shadowing the diffuse cache again.
- Probe environment excludes the solar disk. Probe-hit direct light is a bounce
  at the camera receiver, so adding camera direct sunlight is not a duplicate
  direct term. The per-observation history implementation is unchanged.
- Shared ray queries honor the .35 cutout threshold, use mip-zero crisp alpha
  at the default mip, and intentionally transmit water/glass. Sky transmission
  continues past foliage to test opaque geometry; it does not terminate on the
  first foliage hit. This audit did not execute alpha/AS GPU fixtures.
- Fluid reflections are still the restored stylized single-ray reflection.
  Smoothness affects glint intensity, but there is no roughness-lobe integration
  or filtered rough-reflection hierarchy. Reflected local direct-light sampling
  also remains absent; DDGI indirect is shared, complete PBR parity is not.
- Moment visibility can still misclassify thin walls, high-variance directions,
  relocation errors, and geometry absent from the resident acceleration
  structure. A miss is not proof of sky when a wall lies outside residency or
  the query range. Larger GI range does not create missing geometry. Reflection
  range misses still use sky, and removal of the hit-to-sky fade can expose a
  discontinuity at the query cutoff. These require separate resident/range
  policy and actual GPU scene evidence, not a claim of leak-free lighting.

## Executed checks

`python tools/validation/validate_ddgi_leak.py`:

- Executes the actual production `ddgi_sample` compiled by Slang to C++, across
  six signed normals and spacings 1, 4, 8, 16, 64: constant open field, entirely
  blocked bright cage, one visible corner, and a visible dark corner surrounded
  by blocked bright corners.
- Exercises blocked fine/bright coarse, pending fine handoff, inactive solid,
  valid dark, and entirely pending cages.
- Two independent negative controls alter copies of production source: removing
  absolute support fails the all-blocked fixture; restoring the visibility floor
  fails bright/dark isolation. Neither modifies live shader files.
- Compiles production DDGI trace, RT HDR composite, RT fluid forward shading,
  and the GPU sampling fixture to DXIL, SPIR-V, and Metal. Emitted Metal is not
  macOS execution.

Existing `validate_ddgi_response.py` also passed unchanged, including the
per-observation sparse-history variance tests, step response, reversed-distance
negative control and history negative controls.

No full native build or GPU execution was performed by this shader work unit;
the coordinating session owns serial GPU qualification. The CPU fixture proves
sampling arithmetic on authored probe buffers, not production ray queries,
resident geometry publication, converged scenes, screenshots, or performance.
All touched source files are below 500 lines. Public settings are unchanged.

A dedicated GPU-buffer fixture is wired into `octaryn_client_world_mesh_probe
--ddgi-leak-only`, staged by `octaryn_stage_client_world_mesh_probe`. It runs the
same production sampling shader on 120 direction/spacing/visibility cases,
checks readback and graphics validation, and explicitly reports that scene
traversal is not exercised. Its C++ file passed a syntax-only compile using the
existing native target's compiler flags. Native linking and execution remain
for the coordinating session; it is not evidence of a GPU pass until run.
